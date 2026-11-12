#include "EffectsProcessor.h"

namespace zk
{
void EffectsProcessor::prepare(double sampleRate, int maxBlock)
{
    sr = sampleRate;
    maxBlockSize = juce::jmax(16, maxBlock);

    juce::dsp::ProcessSpec mono { sr, (juce::uint32) maxBlockSize, 1 };
    juce::dsp::ProcessSpec stereo { sr, (juce::uint32) maxBlockSize, 2 };
    for (auto& f : filter) f.prepare(mono);

    chorus.prepare(stereo);
    chorus.setMix(1.0f);
    chorus.setCentreDelay(7.0f);
    chorus.setFeedback(0.0f);

    wet.setSize(2, maxBlockSize, false, true, false);

    delayLen = (int) (sr * 2.0) + 4;
    for (auto& d : delayBuf) d.assign((size_t) delayLen, 0.0f);
    writePos = 0;

    cutoffSm.reset(sr, 0.02);
    cutoffSm.setCurrentAndTargetValue(20000.f);
    delaySm.reset(sr, 0.05);
    delaySm.setCurrentAndTargetValue((float) (0.3 * sr));
    reverb.setSampleRate(sr);
    reset();
}

void EffectsProcessor::reset()
{
    for (auto& f : filter) f.reset();
    chorus.reset();
    reverb.reset();
    for (auto& d : delayBuf) std::fill(d.begin(), d.end(), 0.0f);
    mixCho = mixDly = mixRev = Mix();
    delayActive = false;
}

void EffectsProcessor::blend(Mix& m, juce::AudioBuffer<float>& b, int n, float dry, float w)
{
    const bool useWet = (w > 0.001f || m.wet > 0.001f);
    for (int ch = 0; ch < 2; ++ch)
    {
        b.applyGainRamp(ch, 0, n, m.dry, dry);
        if (useWet)
            b.addFromWithRamp(ch, 0, wet.getReadPointer(ch), n, m.wet, w);
    }
    m.dry = dry;
    m.wet = w;
}

void EffectsProcessor::process(juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const int n = buffer.getNumSamples();
    if (n <= 0 || n > maxBlockSize || buffer.getNumChannels() < 2) return;

    float* ch[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };

    // ---------------- Filtro (global, post-sampler) ----------------
    const bool bypass = (s.filterType == 0 && s.cutoff >= 19000.f);
    if (bypass)
        cutoffSm.setCurrentAndTargetValue(s.cutoff);
    else
    {
        using T = juce::dsp::StateVariableTPTFilterType;
        const auto type = s.filterType == 0 ? T::lowpass : (s.filterType == 1 ? T::highpass : T::bandpass);
        const float q = juce::jmap(s.resonance, 0.0f, 1.0f, 0.707f, 12.0f);
        for (auto& f : filter) { f.setType(type); f.setResonance(q); }
        cutoffSm.setTargetValue(juce::jlimit(20.0f, 20000.0f, s.cutoff));

        const float nyq = (float) (sr * 0.45);
        for (int i = 0; i < n; i += 32)
        {
            const int c = juce::jmin(32, n - i);
            const float fc = juce::jmin(nyq, cutoffSm.skip(c));
            filter[0].setCutoffFrequency(fc);
            filter[1].setCutoffFrequency(fc);
            for (int k = 0; k < 2; ++k)
                for (int j = i; j < i + c; ++j)
                    ch[k][j] = filter[k].processSample(0, ch[k][j]);
        }
    }

    // ---------------- Chorus ----------------
    {
        const float w = s.choWet * s.choSend;
        if (w > 0.001f)
        {
            for (int k = 0; k < 2; ++k) wet.copyFrom(k, 0, buffer, k, 0, n);
            chorus.setRate(juce::jmap(s.choRate, 0.0f, 1.0f, 0.1f, 4.0f));
            chorus.setDepth(s.choDepth);
            juce::dsp::AudioBlock<float> blk(wet);
            auto sub = blk.getSubBlock(0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx(sub);
            chorus.process(ctx);
        }
        else if (mixCho.wet > 0.001f)
            wet.clear(0, n);
        blend(mixCho, buffer, n, s.choDry, w);
    }

    // ---------------- Delay ----------------
    {
        const float w = s.dlyWet * s.dlySend;
        if (w > 0.001f)
        {
            delayActive = true;
            delaySm.setTargetValue((float) (juce::jmap(s.dlyTime, 0.0f, 1.0f, 0.05f, 1.0f) * sr));
            const float fb = juce::jlimit(0.0f, 0.9f, s.dlyFeedback);
            for (int i = 0; i < n; ++i)
            {
                const float d = juce::jlimit(1.0f, (float) (delayLen - 2), delaySm.getNextValue());
                float rp = (float) writePos - d;
                if (rp < 0.0f) rp += (float) delayLen;
                const int i0 = (int) rp;
                const int i1 = (i0 + 1) % delayLen;
                const float fr = rp - (float) i0;
                for (int k = 0; k < 2; ++k)
                {
                    const float out = delayBuf[k][(size_t) i0] * (1.0f - fr) + delayBuf[k][(size_t) i1] * fr;
                    delayBuf[k][(size_t) writePos] = ch[k][i] + out * fb;
                    wet.getWritePointer(k)[i] = out;
                }
                writePos = (writePos + 1) % delayLen;
            }
        }
        else
        {
            if (delayActive)
            {
                for (auto& d : delayBuf) std::fill(d.begin(), d.end(), 0.0f);
                delayActive = false;
            }
            if (mixDly.wet > 0.001f) wet.clear(0, n);
        }
        blend(mixDly, buffer, n, s.dlyDry, w);
    }

    // ---------------- Reverb ----------------
    {
        const float w = s.revWet * s.revSend;
        if (w > 0.001f)
        {
            for (int k = 0; k < 2; ++k) wet.copyFrom(k, 0, buffer, k, 0, n);
            juce::Reverb::Parameters rp;
            rp.roomSize = s.revTime; rp.damping = s.revDamp;
            rp.wetLevel = 0.33f; rp.dryLevel = 0.0f; rp.width = 1.0f; rp.freezeMode = 0.0f;
            reverb.setParameters(rp);
            reverb.processStereo(wet.getWritePointer(0), wet.getWritePointer(1), n);
        }
        else if (mixRev.wet > 0.001f)
            wet.clear(0, n);
        blend(mixRev, buffer, n, s.revDry, w);
    }
}
} // namespace zk

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace zk
{
// Cadena: Filtro -> Chorus -> Delay -> Reverb. Sin asignaciones en process().
class EffectsProcessor
{
public:
    struct Settings
    {
        float cutoff = 20000.f, resonance = 0.15f; int filterType = 0;
        float revDry = 1.f, revWet = 0.f, revTime = 0.5f, revDamp = 0.5f, revSend = 1.f;
        float choDry = 1.f, choWet = 0.f, choRate = 0.3f, choDepth = 0.3f, choSend = 1.f;
        float dlyDry = 1.f, dlyWet = 0.f, dlyTime = 0.3f, dlyFeedback = 0.3f, dlySend = 1.f;
    };

    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const Settings& s); // buffer de 2 canales, <= maxBlock

private:
    struct Mix { float dry = 1.f, wet = 0.f; };
    void blend(Mix& m, juce::AudioBuffer<float>& b, int n, float dry, float wetGain);

    double sr = 44100.0;
    int maxBlockSize = 512;

    juce::dsp::StateVariableTPTFilter<float> filter[2];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSm;
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;
    juce::AudioBuffer<float> wet;

    std::vector<float> delayBuf[2];
    int delayLen = 0, writePos = 0;
    bool delayActive = false;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySm;

    Mix mixCho, mixDly, mixRev;
};
} // namespace zk

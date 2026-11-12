#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace zk
{
namespace pid
{
inline const juce::String master{"master"}, pan{"pan"}, volume{"volume"},
    reverbSend{"reverbSend"}, chorusSend{"chorusSend"}, delaySend{"delaySend"},
    attack{"attack"}, decay{"decay"}, sustain{"sustain"}, release{"release"},
    cutoff{"cutoff"}, resonance{"resonance"}, filterType{"filterType"},
    revDry{"revDry"}, revWet{"revWet"}, revTime{"revTime"}, revDamp{"revDamp"},
    choDry{"choDry"}, choWet{"choWet"}, choRate{"choRate"}, choDepth{"choDepth"},
    dlyDry{"dlyDry"}, dlyWet{"dlyWet"}, dlyTime{"dlyTime"}, dlyFeedback{"dlyFeedback"},
    polyphony{"polyphony"}, glide{"glide"}, glideOn{"glideOn"},
    transpose{"transpose"}, octave{"octave"}, tune{"tune"};
}

inline const int polyphonyChoices[4] = {16, 32, 64, 128};

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto f = [&](const String& id, const String& name, float lo, float hi, float def, float skew = 1.0f)
    {
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name,
                                                    NormalisableRange<float>(lo, hi, 0.0f, skew), def));
    };

    // Master / Sound Control
    f(pid::master,     "Master Volume", 0.f, 1.f, 0.80f);
    f(pid::pan,        "Pan",          -1.f, 1.f, 0.0f);
    f(pid::volume,     "Volume",        0.f, 1.f, 0.85f);
    f(pid::reverbSend, "Reverb Send",   0.f, 1.f, 1.0f);
    f(pid::chorusSend, "Chorus Send",   0.f, 1.f, 1.0f);
    f(pid::delaySend,  "Delay Send",    0.f, 1.f, 1.0f);

    // Envelope
    f(pid::attack,  "Attack",  0.f, 1.f, 0.f);
    f(pid::decay,   "Decay",   0.f, 1.f, 0.f);
    f(pid::sustain, "Sustain", 0.f, 1.f, 1.f);
    f(pid::release, "Release", 0.f, 1.f, 0.f);

    // Filter
    f(pid::cutoff,    "Cutoff",    20.f, 20000.f, 20000.f, 0.25f);
    f(pid::resonance, "Resonance", 0.f, 1.f, 0.15f);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{pid::filterType, 1}, "Filter Type",
                                                 StringArray{"Low-pass", "High-pass", "Band-pass"}, 0));

    // Effects
    f(pid::revDry,  "Reverb Dry",  0.f, 1.f, 1.0f);
    f(pid::revWet,  "Reverb Wet",  0.f, 1.f, 0.30f);
    f(pid::revTime, "Reverb Time", 0.f, 1.f, 0.55f);
    f(pid::revDamp, "Reverb Damp", 0.f, 1.f, 0.50f);
    f(pid::choDry,  "Chorus Dry",  0.f, 1.f, 1.0f);
    f(pid::choWet,  "Chorus Wet",  0.f, 1.f, 0.0f);
    f(pid::choRate, "Chorus Rate", 0.f, 1.f, 0.30f);
    f(pid::choDepth,"Chorus Depth",0.f, 1.f, 0.35f);
    f(pid::dlyDry,  "Delay Dry",   0.f, 1.f, 1.0f);
    f(pid::dlyWet,  "Delay Wet",   0.f, 1.f, 0.0f);
    f(pid::dlyTime, "Delay Time",  0.f, 1.f, 0.35f);
    f(pid::dlyFeedback, "Delay Feedback", 0.f, 1.f, 0.35f);

    // Global
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{pid::polyphony, 1}, "Polyphony",
                                                 StringArray{"16", "32", "64", "128"}, 2));
    f(pid::glide, "Glide", 0.f, 1.f, 0.25f);
    l.add(std::make_unique<AudioParameterBool>(ParameterID{pid::glideOn, 1}, "Glide Switch", false));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{pid::transpose, 1}, "Transpose", -12, 12, 0));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{pid::octave, 1}, "Octave", -2, 2, 0));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{pid::tune, 1}, "Fine Tune (cents)", -100, 100, 0));

    return l;
}
} // namespace zk

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace zk
{
struct ZayPreset
{
    juce::String name;
    bool factory = false;
    juce::String sf2Path;      // "<factory>" o ruta absoluta
    int bank = 0, program = 0;
    std::vector<std::pair<juce::String, float>> values; // id -> valor real
};

class PresetManager
{
public:
    PresetManager();
    const std::vector<ZayPreset>& getAll() const { return presets; }
    void refreshUserPresets();
    // devuelve el indice del preset guardado o -1
    int saveUser(const juce::String& name, juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& sf2Path, int bank, int program);
    static void applyValues(juce::AudioProcessorValueTreeState& apvts, const ZayPreset& p);
    static juce::File getPresetFolder();

private:
    void buildFactory();
    std::vector<ZayPreset> presets;
    int numFactory = 0;
};
} // namespace zk

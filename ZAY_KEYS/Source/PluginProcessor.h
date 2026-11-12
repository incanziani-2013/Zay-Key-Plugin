#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <memory>
#include "Parameters.h"
#include "SoundFontEngine.h"
#include "EffectsProcessor.h"
#include "PresetManager.h"

namespace zk
{
inline const juce::String kFactoryPath = "<factory>";
inline const juce::String kFactoryName = "Zay Grand x - 2009.SF2";
enum class LoadState { Empty, Loading, Ready, Error };
}

class ZayKeysProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster,
                         private juce::Timer
{
public:
    ZayKeysProcessor();
    ~ZayKeysProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ZAY KEYS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- interfaz / LCD (hilo de mensajes) ----
    juce::AudioProcessorValueTreeState apvts;
    zk::PresetManager presets;

    std::shared_ptr<const zk::FontInfo> getFontInfo() const { return fontInfo; }
    int getBank() const { return curBank; }
    int getProgram() const { return curProgram; }
    int getPresetIndex() const { return presetIndex; }
    juce::String getPresetName() const;
    zk::LoadState getLoadState() const { return loadState; }
    juce::String getStatusText() const { return statusText; }
    juce::String getLastDirectory() const;
    int getVoiceCount() const { return engine.getVoiceCount(); }
    int getVoiceLimit() const { return engine.getVoiceLimit(); }
    int getKeyMin() const { return engine.getCurrentMinKey(); }
    int getKeyMax() const { return engine.getCurrentMaxKey(); }

    void loadPreset(int index);
    void selectInstrument(int bank, int program);
    void stepInstrument(int delta);
    void stepBank(int delta);
    void stepPreset(int delta);
    int  getBankIndex() const;
    void loadSoundFontFile(const juce::File& f);
    bool saveUserPreset(const juce::String& name);
    void panic() { panicRequested.store(true); }
    void nudgeParam(const juce::String& id, int steps);   // +-1 paso en parametros enteros

    // teclado virtual -> audio (lock-free)
    bool pushUiNote(bool on, int note, float velocity);
    uint64_t getActiveMask(int half) const { return activeMask[half].load(); }
    float getMeterL() const { return meterL.load(); }
    float getMeterR() const { return meterR.load(); }

private:
    struct UiNote { int note; float vel; bool on; };

    void timerCallback() override;
    void loadDesired();
    void onFontLoaded(zk::LoadedFont* font, const juce::String& err, int id);
    void setStatus(zk::LoadState s, const juce::String& t);
    void restoreFromState(const juce::ValueTree& vt);
    void handleNote(bool on, int key, float vel);
    void handleMidi(const juce::MidiMessage& m);
    zk::EffectsProcessor::Settings readFxSettings() const;
    std::atomic<float>* raw(const juce::String& id) const { return apvts.getRawParameterValue(id); }

    zk::SoundFontEngine engine;
    zk::EffectsProcessor fx;

    std::shared_ptr<const zk::FontInfo> fontInfo;
    juce::String desiredPath = zk::kFactoryPath;
    int desiredBank = 0, desiredProgram = 0;
    int curBank = 0, curProgram = 0, presetIndex = 0;
    juce::String customPresetName;
    zk::LoadState loadState = zk::LoadState::Empty;
    juce::String statusText;
    juce::uint32 errorSince = 0;
    std::atomic<int> loadRequestId { 0 };
    double currentSR = 0.0;
    int maxBlock = 512;

    juce::AbstractFifo uiFifo { 256 };
    UiNote uiBuf[256];
    std::atomic<uint64_t> activeMask[2];
    std::atomic<bool> panicRequested { false };
    int soundingKey[128];
    std::atomic<float> meterL { 0.f }, meterR { 0.f };
    juce::SmoothedValue<float> gainL, gainR;

    juce::ThreadPool loadPool { 1 };
    JUCE_DECLARE_WEAK_REFERENCEABLE(ZayKeysProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZayKeysProcessor)
};

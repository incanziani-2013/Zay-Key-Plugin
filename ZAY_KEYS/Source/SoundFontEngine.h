#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <vector>

struct tsf; // TinySoundFont (MIT) - ver third_party/tsf.h

namespace zk
{
struct InstrumentEntry
{
    int bank = 0, program = 0, tsfIndex = 0;
    int minKey = 0, maxKey = 127, layers = 1;
    juce::String name;
};

// Un SoundFont ya cargado y listo para el hilo de audio.
struct LoadedFont
{
    tsf* handle = nullptr;
    juce::String name;
    std::vector<InstrumentEntry> entries;
    ~LoadedFont();
};

// Datos de solo lectura para la interfaz (hilo de mensajes).
struct FontInfo
{
    juce::String name, path;
    double sampleRate = 0.0;
    std::vector<InstrumentEntry> entries;
};

/**  Motor SF2 sobre TinySoundFont.
     - load(): se llama en un hilo de fondo (nunca en el hilo de audio).
     - publish()/collectGarbage()/request*(): hilo de mensajes.
     - noteOn/noteOff/render/...: SOLO hilo de audio. Sin asignaciones de memoria ni bloqueos. */
class SoundFontEngine
{
public:
    SoundFontEngine();
    ~SoundFontEngine();

    // ---- hilo de fondo ----
    static LoadedFont* load(const void* data, size_t size, double sampleRate,
                            const juce::String& displayName, juce::String& error);

    // ---- hilo de mensajes ----
    void prepare(double sampleRate, int maxBlock);
    void publish(LoadedFont* font);            // toma posesion
    void collectGarbage();                     // borra fuentes retiradas
    void requestInstrument(int bank, int program) { requested.store((bank << 16) | (program & 0xffff)); }
    void setEnvelope(float a, float d, float s, float r);  // 0..1 normalizados
    void setPolyphony(int notes)   { polyNotes.store(notes); }
    void setGlide(bool on, float amount) { glideOn.store(on); glideAmount.store(amount); }
    void setTuneCents(float c)     { tuneCents.store(c); }
    int  getVoiceCount() const     { return voiceCount.load(); }
    int  getVoiceLimit() const     { return voiceLimit.load(); }
    int  getCurrentLayers() const  { return curLayers.load(); }
    int  getCurrentMinKey() const  { return curMin.load(); }
    int  getCurrentMaxKey() const  { return curMax.load(); }

    // ---- hilo de audio ----
    void beginBlock();
    void noteOn(int key, float velocity);
    void noteOff(int key);
    void pitchBend(int value14);
    void sustain(bool on);
    void allNotesOff();
    void soundsOff();
    void render(float* left, float* right, int numSamples);

private:
    struct GlideVoice { int index; unsigned playIndex; float offset; };

    int  mapKey(int key) const;
    void applyInstrument(int req);
    void stealIfNeeded();
    void postProcessNewVoices(int key, float vel);
    void updateGlide(int numSamples);
    float channelShiftSemis() const;
    void retire(LoadedFont* f);

    std::atomic<LoadedFont*> pending { nullptr };
    LoadedFont* active = nullptr;
    static constexpr int kRetireSlots = 8;
    std::atomic<LoadedFont*> retired[kRetireSlots];

    std::atomic<int> requested { -1 };
    int appliedRequest = -2;

    std::atomic<float> envA { 0.f }, envD { 0.f }, envS { 1.f }, envR { 0.f };
    std::atomic<int> polyNotes { 64 };
    std::atomic<bool> glideOn { false };
    std::atomic<float> glideAmount { 0.f };
    std::atomic<float> tuneCents { 0.f };
    float appliedTune = 0.f;

    std::atomic<int> voiceCount { 0 }, voiceLimit { 64 }, curLayers { 1 }, curMin { 0 }, curMax { 127 };

    double sr = 44100.0;
    int maxBlockSize = 512;
    std::vector<float> interleaved;

    int lastKey = -1;
    int pitchWheel = 8192;
    static constexpr int kMaxGlide = 64;
    GlideVoice glideVoices[kMaxGlide];
    int glideCount = 0;
    int layers = 1, minKey = 0, maxKey = 127;
};
} // namespace zk

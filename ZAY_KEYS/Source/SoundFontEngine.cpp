// El unico lugar donde se compila la implementacion de TinySoundFont.
// Se incluye aqui (y no como libreria) para poder ajustar envolventes y glide por voz.
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "tsf.h"

#include "SoundFontEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace zk
{
static constexpr int kMaxVoices = 512;

LoadedFont::~LoadedFont()
{
    if (handle != nullptr)
        tsf_close(handle);
}

SoundFontEngine::SoundFontEngine()
{
    for (auto& r : retired) r.store(nullptr);
}

SoundFontEngine::~SoundFontEngine()
{
    delete pending.exchange(nullptr);
    delete active;
    for (auto& r : retired) delete r.exchange(nullptr);
}

// ------------------------------------------------------------------ carga (hilo de fondo)
LoadedFont* SoundFontEngine::load(const void* data, size_t size, double sampleRate,
                                  const juce::String& displayName, juce::String& error)
{
    const auto* bytes = static_cast<const unsigned char*>(data);

    if (size < 12 || std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "sfbk", 4) != 0)
    {
        error = "El archivo no es un SoundFont 2 (.sf2) valido";
        return nullptr;
    }
    if (size > 0x7fffffffu)
    {
        error = "SoundFont demasiado grande (max 2 GB)";
        return nullptr;
    }

    tsf* f = tsf_load_memory(data, (int) size);
    if (f == nullptr)
    {
        error = "No se pudo interpretar el SoundFont (archivo danado o formato no soportado)";
        return nullptr;
    }

    const int count = tsf_get_presetcount(f);
    if (count <= 0)
    {
        tsf_close(f);
        error = "El SoundFont no contiene presets";
        return nullptr;
    }

    tsf_set_output(f, TSF_STEREO_INTERLEAVED, (int) sampleRate, 0.0f);
    if (! tsf_set_max_voices(f, kMaxVoices))   // se reservan TODAS las voces ahora: el audio nunca asigna memoria
    {
        tsf_close(f);
        error = "Memoria insuficiente";
        return nullptr;
    }
    tsf_channel_set_presetindex(f, 0, 0);       // crea el canal 0 ahora (asigna memoria) y no en audio
    tsf_channel_set_pitchrange(f, 0, 2.0f);

    auto* lf = new LoadedFont();
    lf->handle = f;
    lf->name = displayName;

    for (int i = 0; i < count; ++i)
    {
        const tsf_preset& p = f->presets[i];
        InstrumentEntry e;
        e.bank = p.bank;
        e.program = p.preset;
        e.tsfIndex = i;
        e.name = juce::String::fromUTF8(p.presetName, (int) strnlen(p.presetName, 20)).trim();
        if (e.name.isEmpty()) e.name = "Preset " + juce::String(i);

        int lo = 127, hi = 0, lay = 1;
        for (int r = 0; r < p.regionNum; ++r)
        {
            lo = std::min<int>(lo, p.regions[r].lokey);
            hi = std::max<int>(hi, p.regions[r].hikey);
        }
        for (int probe : { 36, 60, 84 })
        {
            int c = 0;
            for (int r = 0; r < p.regionNum; ++r)
                if (probe >= p.regions[r].lokey && probe <= p.regions[r].hikey) ++c;
            lay = std::max(lay, c);
        }
        if (lo > hi) { lo = 0; hi = 127; }
        e.minKey = lo; e.maxKey = hi; e.layers = juce::jlimit(1, 8, lay);
        lf->entries.push_back(e);
    }

    std::sort(lf->entries.begin(), lf->entries.end(), [](const InstrumentEntry& a, const InstrumentEntry& b)
              { return a.bank != b.bank ? a.bank < b.bank : a.program < b.program; });
    return lf;
}

// ------------------------------------------------------------------ hilo de mensajes
void SoundFontEngine::prepare(double sampleRate, int maxBlock)
{
    sr = sampleRate;
    maxBlockSize = std::max(16, maxBlock);
    interleaved.assign((size_t) maxBlockSize * 2, 0.0f);
}

void SoundFontEngine::publish(LoadedFont* font)
{
    // si habia otra pendiente sin consumir, la reemplazamos y la borramos aqui (hilo de mensajes)
    delete pending.exchange(font);
}

void SoundFontEngine::collectGarbage()
{
    for (auto& r : retired)
        delete r.exchange(nullptr);
}

void SoundFontEngine::setEnvelope(float a, float d, float s, float r)
{
    // knobs normalizados -> segundos (curva cuadratica)
    envA.store(a * a * 4.0f);
    envD.store(d * d * 6.0f);
    envS.store(juce::jlimit(0.0f, 1.0f, s));
    envR.store(r * r * 5.0f);
}

// ------------------------------------------------------------------ hilo de audio
void SoundFontEngine::retire(LoadedFont* f)
{
    for (auto& slot : retired)
    {
        LoadedFont* expected = nullptr;
        if (slot.compare_exchange_strong(expected, f))
            return;
    }
    // 8 fuentes pendientes de borrar sin que el hilo de mensajes las recoja: extremadamente improbable.
    // Se prefiere una fuga puntual antes que liberar memoria en el hilo de audio.
}

void SoundFontEngine::beginBlock()
{
    if (LoadedFont* p = pending.exchange(nullptr))
    {
        if (active != nullptr) retire(active);
        active = p;
        glideCount = 0;
        lastKey = -1;
        appliedRequest = -2;
        pitchWheel = 8192;
        appliedTune = 0.0f;
    }

    if (active == nullptr)
    {
        voiceCount.store(0);
        return;
    }

    const int req = requested.load();
    if (req != appliedRequest && req >= 0)
    {
        applyInstrument(req);
        appliedRequest = req;
    }
    else if (appliedRequest == -2 && ! active->entries.empty())
    {
        applyInstrument((active->entries[0].bank << 16) | (active->entries[0].program & 0xffff));
        appliedRequest = -3;
    }

    const float t = tuneCents.load();
    if (t != appliedTune)
    {
        appliedTune = t;
        tsf_channel_set_tuning(active->handle, 0, t / 100.0f);
    }
}

void SoundFontEngine::applyInstrument(int req)
{
    const int bank = req >> 16, prog = req & 0xffff;
    for (const auto& e : active->entries)
    {
        if (e.bank == bank && e.program == prog)
        {
            tsf_channel_set_presetindex(active->handle, 0, e.tsfIndex);
            minKey = e.minKey; maxKey = e.maxKey; layers = e.layers;
            curMin.store(minKey); curMax.store(maxKey); curLayers.store(layers);
            return;
        }
    }
}

int SoundFontEngine::mapKey(int key) const
{
    // Si el SoundFont no cubre toda la extension de 88 teclas, se pliega por octavas
    // (asi las teclas fuera de rango suenan en lugar de quedar mudas).
    if (maxKey - minKey < 12) return key;
    while (key < minKey) key += 12;
    while (key > maxKey) key -= 12;
    return key;
}

float SoundFontEngine::channelShiftSemis() const
{
    const float range = 2.0f;
    const float bend = (pitchWheel == 8192) ? 0.0f : (pitchWheel / 16383.0f * range * 2.0f) - range;
    return bend + appliedTune / 100.0f;
}

void SoundFontEngine::stealIfNeeded()
{
    tsf* f = active->handle;
    const int limit = juce::jlimit(1, kMaxVoices, polyNotes.load() * layers);
    voiceLimit.store(limit);

    if (tsf_active_voice_count(f) < limit)
        return;

    // 1) la voz mas vieja que aun no esta en release -> release rapido
    tsf_voice* oldest = nullptr;
    for (int i = 0; i < f->voiceNum; ++i)
    {
        tsf_voice* v = &f->voices[i];
        if (v->playingPreset == -1 || v->ampenv.segment >= TSF_SEGMENT_RELEASE) continue;
        if (oldest == nullptr || v->playIndex < oldest->playIndex) oldest = v;
    }
    if (oldest != nullptr)
    {
        // se retiran todas las voces de esa misma nota (capas L/R)
        const unsigned idx = oldest->playIndex;
        for (int i = 0; i < f->voiceNum; ++i)
        {
            tsf_voice* v = &f->voices[i];
            if (v->playingPreset != -1 && v->playIndex == idx)
                tsf_voice_endquick(f, v);
        }
        return;
    }

    // 2) todas ya estan en release: se corta la que lleva mas tiempo
    for (int i = 0; i < f->voiceNum; ++i)
    {
        tsf_voice* v = &f->voices[i];
        if (v->playingPreset == -1) continue;
        if (oldest == nullptr || v->playIndex < oldest->playIndex) oldest = v;
    }
    if (oldest != nullptr)
        tsf_voice_kill(oldest);
}

void SoundFontEngine::postProcessNewVoices(int key, float vel)
{
    tsf* f = active->handle;
    const unsigned newIndex = f->voicePlayIndex - 1;

    const float a = envA.load(), d = envD.load(), s = envS.load(), r = envR.load();
    const bool envTouched = (a > 0.0f || d > 0.0f || s < 0.999f || r > 0.0f);
    const bool doGlide = glideOn.load() && lastKey >= 0 && lastKey != key && glideAmount.load() > 0.001f;
    const float shift = channelShiftSemis();

    for (int i = 0; i < f->voiceNum; ++i)
    {
        tsf_voice* v = &f->voices[i];
        if (v->playingPreset == -1 || v->playIndex != newIndex || v->playingKey != key) continue;

        if (envTouched)
        {
            tsf_envelope p = v->region->ampenv;
            p.attack += a;
            if (p.keynumToDecay == 0.0f) p.decay = std::max(p.decay, d);
            p.sustain *= s;
            p.release += r;
            tsf_voice_envelope_setup(&v->ampenv, &p, key, (short) (vel * 127), TSF_TRUE, f->outSampleRate);
        }

        if (doGlide && glideCount < kMaxGlide)
        {
            const float offset = (float) juce::jlimit(-24, 24, lastKey - key);
            glideVoices[glideCount++] = { i, v->playIndex, offset };
            tsf_voice_calcpitchratio(v, shift + offset, f->outSampleRate);
        }
    }
}

void SoundFontEngine::updateGlide(int numSamples)
{
    if (glideCount == 0 || active == nullptr) return;
    tsf* f = active->handle;

    // amount 0..1 -> constante de tiempo 0.02..0.6 s
    const float tau = 0.02f + glideAmount.load() * 0.58f;
    const float k = std::exp(-(float) numSamples / ((float) sr * tau));
    const float shift = channelShiftSemis();

    int w = 0;
    for (int i = 0; i < glideCount; ++i)
    {
        GlideVoice g = glideVoices[i];
        tsf_voice* v = &f->voices[g.index];
        const bool alive = (g.index < f->voiceNum) && v->playingPreset != -1 && v->playIndex == g.playIndex;
        if (! alive) continue;

        g.offset *= k;
        if (std::abs(g.offset) < 0.02f)
            tsf_voice_calcpitchratio(v, shift, f->outSampleRate);
        else
        {
            tsf_voice_calcpitchratio(v, shift + g.offset, f->outSampleRate);
            glideVoices[w++] = g;
        }
    }
    glideCount = w;
}

void SoundFontEngine::noteOn(int key, float velocity)
{
    if (active == nullptr) return;
    const int k = mapKey(key);
    stealIfNeeded();
    tsf_channel_note_on(active->handle, 0, k, juce::jlimit(0.0f, 1.0f, velocity));
    postProcessNewVoices(k, velocity);
    lastKey = k;
}

void SoundFontEngine::noteOff(int key)
{
    if (active == nullptr) return;
    tsf_channel_note_off(active->handle, 0, mapKey(key));
}

void SoundFontEngine::pitchBend(int value14)
{
    if (active == nullptr) return;
    pitchWheel = juce::jlimit(0, 16383, value14);
    tsf_channel_set_pitchwheel(active->handle, 0, pitchWheel);
}

void SoundFontEngine::sustain(bool on)
{
    if (active == nullptr) return;
    tsf_channel_set_sustain(active->handle, 0, on ? 1 : 0);
}

void SoundFontEngine::allNotesOff()
{
    if (active == nullptr) return;
    tsf_channel_note_off_all(active->handle, 0);
}

void SoundFontEngine::soundsOff()
{
    if (active == nullptr) return;
    tsf_channel_sounds_off_all(active->handle, 0);
    glideCount = 0;
}

void SoundFontEngine::render(float* left, float* right, int numSamples)
{
    if (active == nullptr)
        return;

    tsf* f = active->handle;
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min(numSamples - done, maxBlockSize);
        updateGlide(n);
        tsf_render_float(f, interleaved.data(), n, 0);
        const float* src = interleaved.data();
        for (int i = 0; i < n; ++i)
        {
            left[done + i]  += src[2 * i];
            right[done + i] += src[2 * i + 1];
        }
        done += n;
    }
    voiceCount.store(tsf_active_voice_count(f));
}
} // namespace zk

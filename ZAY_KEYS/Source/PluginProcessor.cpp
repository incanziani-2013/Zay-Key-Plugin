#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

using namespace zk;

ZayKeysProcessor::ZayKeysProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ZAYKEYS", createLayout())
{
    activeMask[0].store(0); activeMask[1].store(0);
    for (auto& k : soundingKey) k = -1;
    apvts.state.setProperty("sf2Path", kFactoryPath, nullptr);
    startTimer(250);
}

ZayKeysProcessor::~ZayKeysProcessor()
{
    stopTimer();
    loadPool.removeAllJobs(true, 4000);
}

bool ZayKeysProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ------------------------------------------------------------------ estado de carga
void ZayKeysProcessor::setStatus(LoadState s, const juce::String& t)
{
    loadState = s;
    statusText = t;
    if (s == LoadState::Error) errorSince = juce::Time::getMillisecondCounter();
    sendChangeMessage();
}

void ZayKeysProcessor::timerCallback()
{
    engine.collectGarbage();
    if (loadState == LoadState::Error && fontInfo != nullptr
        && juce::Time::getMillisecondCounter() - errorSince > 6000)
        setStatus(LoadState::Ready, "OK");
}

juce::String ZayKeysProcessor::getPresetName() const
{
    if (customPresetName.isNotEmpty()) return customPresetName;
    auto& all = presets.getAll();
    return juce::isPositiveAndBelow(presetIndex, (int) all.size()) ? all[(size_t) presetIndex].name : juce::String("INIT");
}

juce::String ZayKeysProcessor::getLastDirectory() const
{
    return apvts.state.getProperty("lastDir", "").toString();
}

// ------------------------------------------------------------------ carga asincrona del SF2
void ZayKeysProcessor::loadDesired()
{
    if (currentSR <= 0.0) return;   // se hara en prepareToPlay
    const int id = ++loadRequestId;
    setStatus(LoadState::Loading, "Cargando SoundFont...");

    const juce::String path = desiredPath;
    const double sr = currentSR;
    juce::WeakReference<ZayKeysProcessor> weak(this);

    loadPool.addJob([weak, path, sr, id]()
    {
        juce::String err, display;
        LoadedFont* font = nullptr;
        if (path == kFactoryPath)
        {
            display = kFactoryName;
            font = SoundFontEngine::load(BinaryData::ZayGrand_sf2, (size_t) BinaryData::ZayGrand_sf2Size, sr, display, err);
        }
        else
        {
            juce::File f(path);
            display = f.getFileName();
            juce::MemoryBlock mb;
            if (! f.existsAsFile())            err = "No se encontro: " + f.getFileName();
            else if (! f.loadFileAsData(mb))   err = "No se pudo leer: " + f.getFileName();
            else font = SoundFontEngine::load(mb.getData(), mb.getSize(), sr, display, err);
        }
        juce::MessageManager::callAsync([weak, font, err, id]()
        {
            if (auto* self = weak.get()) self->onFontLoaded(font, err, id);
            else delete font;
        });
    });
}

void ZayKeysProcessor::onFontLoaded(LoadedFont* font, const juce::String& err, int id)
{
    std::unique_ptr<LoadedFont> owned(font);
    if (id != loadRequestId.load()) return;   // llego una peticion mas nueva

    if (owned == nullptr)
    {
        if (fontInfo != nullptr) desiredPath = fontInfo->path;   // volvemos al SF2 anterior
        else if (desiredPath != kFactoryPath) { desiredPath = kFactoryPath; loadDesired(); }
        apvts.state.setProperty("sf2Path", desiredPath, nullptr);
        setStatus(LoadState::Error, "ERROR: " + err);
        return;
    }

    auto info = std::make_shared<FontInfo>();
    info->name = owned->name;
    info->path = desiredPath;
    info->sampleRate = currentSR;
    info->entries = owned->entries;

    // instrumento deseado o, si no existe, el primero
    const InstrumentEntry* pick = &info->entries.front();
    for (auto& e : info->entries)
        if (e.bank == desiredBank && e.program == desiredProgram) { pick = &e; break; }
    curBank = pick->bank; curProgram = pick->program;
    desiredBank = curBank; desiredProgram = curProgram;

    fontInfo = info;
    engine.publish(owned.release());
    engine.requestInstrument(curBank, curProgram);

    apvts.state.setProperty("sf2Path", info->path, nullptr);
    apvts.state.setProperty("bank", curBank, nullptr);
    apvts.state.setProperty("program", curProgram, nullptr);
    setStatus(LoadState::Ready, "OK");
}

void ZayKeysProcessor::loadSoundFontFile(const juce::File& f)
{
    apvts.state.setProperty("lastDir", f.getParentDirectory().getFullPathName(), nullptr);
    desiredPath = f.getFullPathName();
    desiredBank = 0; desiredProgram = 0;
    customPresetName = "USER SF2";
    loadDesired();
}

void ZayKeysProcessor::selectInstrument(int bank, int program)
{
    curBank = bank; curProgram = program;
    desiredBank = bank; desiredProgram = program;
    engine.requestInstrument(bank, program);
    apvts.state.setProperty("bank", bank, nullptr);
    apvts.state.setProperty("program", program, nullptr);
    sendChangeMessage();
}

void ZayKeysProcessor::stepInstrument(int delta)
{
    if (fontInfo == nullptr || fontInfo->entries.empty()) return;
    auto& v = fontInfo->entries;
    int idx = 0;
    for (int i = 0; i < (int) v.size(); ++i)
        if (v[(size_t) i].bank == curBank && v[(size_t) i].program == curProgram) { idx = i; break; }
    idx = (idx + delta + (int) v.size()) % (int) v.size();
    selectInstrument(v[(size_t) idx].bank, v[(size_t) idx].program);
}

int ZayKeysProcessor::getBankIndex() const
{
    if (fontInfo == nullptr) return 0;
    int idx = 0, last = -1;
    for (auto& e : fontInfo->entries)
    {
        if (e.bank != last) { if (last != -1) ++idx; last = e.bank; }
        if (e.bank == curBank) break;
    }
    return idx;
}

void ZayKeysProcessor::stepBank(int delta)
{
    if (fontInfo == nullptr || fontInfo->entries.empty()) return;
    std::vector<int> banks;
    for (auto& e : fontInfo->entries)
        if (banks.empty() || banks.back() != e.bank) banks.push_back(e.bank);
    int bi = 0;
    for (int i = 0; i < (int) banks.size(); ++i) if (banks[(size_t) i] == curBank) bi = i;
    bi = (bi + delta + (int) banks.size()) % (int) banks.size();
    for (auto& e : fontInfo->entries)
        if (e.bank == banks[(size_t) bi]) { selectInstrument(e.bank, e.program); return; }
}

void ZayKeysProcessor::loadPreset(int index)
{
    auto& all = presets.getAll();
    if (! juce::isPositiveAndBelow(index, (int) all.size())) return;
    const auto& p = all[(size_t) index];
    presetIndex = index;
    customPresetName = {};
    PresetManager::applyValues(apvts, p);
    apvts.state.setProperty("presetName", p.name, nullptr);

    desiredBank = p.bank; desiredProgram = p.program;
    if (fontInfo != nullptr && fontInfo->path == p.sf2Path && loadState != LoadState::Loading)
        selectInstrument(p.bank, p.program);
    else
    {
        desiredPath = p.sf2Path;
        loadDesired();
    }
    sendChangeMessage();
}

void ZayKeysProcessor::stepPreset(int delta)
{
    const int n = (int) presets.getAll().size();
    if (n > 0) loadPreset((presetIndex + delta + n) % n);
}

bool ZayKeysProcessor::saveUserPreset(const juce::String& name)
{
    const int idx = presets.saveUser(name, apvts, desiredPath, curBank, curProgram);
    if (idx < 0) { setStatus(LoadState::Error, "ERROR: no se pudo guardar el preset"); return false; }
    presetIndex = idx;
    customPresetName = {};
    apvts.state.setProperty("presetName", presets.getAll()[(size_t) idx].name, nullptr);
    sendChangeMessage();
    return true;
}

void ZayKeysProcessor::nudgeParam(const juce::String& id, int steps)
{
    if (auto* p = apvts.getParameter(id))
    {
        const auto r = p->getNormalisableRange();
        const float cur = p->convertFrom0to1(p->getValue());
        const float step = (r.interval > 0.f) ? r.interval : 1.0f;
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(r.start, r.end, cur + (float) steps * step)));
    }
}

// ------------------------------------------------------------------ audio
void ZayKeysProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSR = sampleRate;
    maxBlock = juce::jmax(16, samplesPerBlock);
    engine.prepare(sampleRate, maxBlock);
    fx.prepare(sampleRate, maxBlock);
    gainL.reset(sampleRate, 0.02); gainR.reset(sampleRate, 0.02);
    gainL.setCurrentAndTargetValue(0.f); gainR.setCurrentAndTargetValue(0.f);
    for (auto& k : soundingKey) k = -1;
    activeMask[0].store(0); activeMask[1].store(0);

    if (fontInfo == nullptr || fontInfo->sampleRate != sampleRate)
        loadDesired();
}

bool ZayKeysProcessor::pushUiNote(bool on, int note, float velocity)
{
    const auto w = uiFifo.write(1);
    if (w.blockSize1 > 0) { uiBuf[w.startIndex1] = { note, velocity, on }; return true; }
    return false;
}

EffectsProcessor::Settings ZayKeysProcessor::readFxSettings() const
{
    EffectsProcessor::Settings s;
    s.cutoff = raw(pid::cutoff)->load();
    s.resonance = raw(pid::resonance)->load();
    s.filterType = (int) raw(pid::filterType)->load();
    s.revDry = raw(pid::revDry)->load();  s.revWet = raw(pid::revWet)->load();
    s.revTime = raw(pid::revTime)->load(); s.revDamp = raw(pid::revDamp)->load();
    s.revSend = raw(pid::reverbSend)->load();
    s.choDry = raw(pid::choDry)->load();  s.choWet = raw(pid::choWet)->load();
    s.choRate = raw(pid::choRate)->load(); s.choDepth = raw(pid::choDepth)->load();
    s.choSend = raw(pid::chorusSend)->load();
    s.dlyDry = raw(pid::dlyDry)->load();  s.dlyWet = raw(pid::dlyWet)->load();
    s.dlyTime = raw(pid::dlyTime)->load(); s.dlyFeedback = raw(pid::dlyFeedback)->load();
    s.dlySend = raw(pid::delaySend)->load();
    return s;
}

void ZayKeysProcessor::handleNote(bool on, int key, float vel)
{
    if (! juce::isPositiveAndBelow(key, 128)) return;

    const int bit = key - 21;
    if (bit >= 0 && bit < 88)
    {
        const int half = bit / 64;
        const uint64_t m = (uint64_t) 1 << (bit % 64);
        const uint64_t cur = activeMask[half].load();
        activeMask[half].store(on ? (cur | m) : (cur & ~m));
    }

    if (on)
    {
        const int off = (int) raw(pid::transpose)->load() + 12 * (int) raw(pid::octave)->load();
        const int sk = key + off;
        if (! juce::isPositiveAndBelow(sk, 128)) return;
        if (soundingKey[key] >= 0) engine.noteOff(soundingKey[key]);
        soundingKey[key] = sk;
        engine.noteOn(sk, vel);
    }
    else if (soundingKey[key] >= 0)
    {
        engine.noteOff(soundingKey[key]);
        soundingKey[key] = -1;
    }
}

void ZayKeysProcessor::handleMidi(const juce::MidiMessage& m)
{
    if (m.isNoteOn())            handleNote(true, m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())      handleNote(false, m.getNoteNumber(), 0.f);
    else if (m.isPitchWheel())   engine.pitchBend(m.getPitchWheelValue());
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 64) engine.sustain(m.getControllerValue() >= 64);
        else if (cc == 123) engine.allNotesOff();
        else if (cc == 120) engine.soundsOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.soundsOff();
}

void ZayKeysProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2 || n == 0) return;

    engine.setEnvelope(raw(pid::attack)->load(), raw(pid::decay)->load(),
                       raw(pid::sustain)->load(), raw(pid::release)->load());
    engine.setPolyphony(polyphonyChoices[juce::jlimit(0, 3, (int) raw(pid::polyphony)->load())]);
    engine.setGlide(raw(pid::glideOn)->load() > 0.5f, raw(pid::glide)->load());
    engine.setTuneCents(raw(pid::tune)->load());
    engine.beginBlock();

    if (panicRequested.exchange(false))
    {
        engine.soundsOff();
        for (auto& k : soundingKey) k = -1;
        activeMask[0].store(0); activeMask[1].store(0);
    }

    // notas del teclado virtual
    {
        const auto r = uiFifo.read(uiFifo.getNumReady());
        for (int i = 0; i < r.blockSize1; ++i) { auto& e = uiBuf[r.startIndex1 + i]; handleNote(e.on, e.note, e.vel); }
        for (int i = 0; i < r.blockSize2; ++i) { auto& e = uiBuf[r.startIndex2 + i]; handleNote(e.on, e.note, e.vel); }
    }

    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit(0, n, meta.samplePosition);
        if (t > pos) { engine.render(l + pos, r + pos, t - pos); pos = t; }
        handleMidi(meta.getMessage());
    }
    if (n > pos) engine.render(l + pos, r + pos, n - pos);
    midi.clear();

    // filtro + efectos (por segmentos <= maxBlock)
    const auto fxs = readFxSettings();
    for (int start = 0; start < n; start += maxBlock)
    {
        const int count = juce::jmin(maxBlock, n - start);
        juce::AudioBuffer<float> seg(buffer.getArrayOfWritePointers(), 2, start, count);
        fx.process(seg, fxs);
    }

    // volumen, master y pan (ley de potencia constante)
    const float vol = raw(pid::volume)->load(), mst = raw(pid::master)->load();
    const float g = vol * vol * mst * mst * 1.5f;
    const float ang = (raw(pid::pan)->load() + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    gainL.setTargetValue(g * std::cos(ang) * 1.41421356f);
    gainR.setTargetValue(g * std::sin(ang) * 1.41421356f);
    float pkL = 0.f, pkR = 0.f;
    for (int i = 0; i < n; ++i)
    {
        l[i] *= gainL.getNextValue();
        r[i] *= gainR.getNextValue();
        pkL = juce::jmax(pkL, std::abs(l[i]));
        pkR = juce::jmax(pkR, std::abs(r[i]));
    }
    const float decay = std::pow(0.9995f, (float) n);
    meterL.store(juce::jmax(pkL, meterL.load() * decay));
    meterR.store(juce::jmax(pkR, meterR.load() * decay));
}

// ------------------------------------------------------------------ estado (FL Studio .fst incluido)
void ZayKeysProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty("sf2Path", desiredPath, nullptr);
    state.setProperty("bank", curBank, nullptr);
    state.setProperty("program", curProgram, nullptr);
    state.setProperty("presetName", getPresetName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void ZayKeysProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml(*xml);
            apvts.replaceState(vt);
            restoreFromState(apvts.state);
        }
}

void ZayKeysProcessor::restoreFromState(const juce::ValueTree& vt)
{
    desiredPath = vt.getProperty("sf2Path", kFactoryPath).toString();
    if (desiredPath.isEmpty()) desiredPath = kFactoryPath;
    desiredBank = (int) vt.getProperty("bank", 0);
    desiredProgram = (int) vt.getProperty("program", 0);

    const auto name = vt.getProperty("presetName", "").toString();
    auto& all = presets.getAll();
    presetIndex = 0; customPresetName = name;
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].name == name) { presetIndex = i; customPresetName = {}; }

    if (fontInfo != nullptr && fontInfo->path == desiredPath && fontInfo->sampleRate == currentSR)
        selectInstrument(desiredBank, desiredProgram);
    else
        loadDesired();   // si aun no hay sample rate, se carga en prepareToPlay
    sendChangeMessage();
}

juce::AudioProcessorEditor* ZayKeysProcessor::createEditor() { return new ZayKeysEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZayKeysProcessor(); }

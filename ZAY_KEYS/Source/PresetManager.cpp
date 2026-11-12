#include "PresetManager.h"
#include "Parameters.h"

namespace zk
{
static const char* kFactoryPathStr = "<factory>";

PresetManager::PresetManager()
{
    buildFactory();
    refreshUserPresets();
}

juce::File PresetManager::getPresetFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("GOLDMINE AUDIO").getChildFile("ZAY KEYS").getChildFile("Presets");
}

void PresetManager::buildFactory()
{
    using V = std::vector<std::pair<juce::String, float>>;

    ZayPreset organ;
    organ.name = "ZAY ORGAN"; organ.factory = true; organ.sf2Path = kFactoryPathStr;
    organ.values = V {
        { pid::master, 0.8f }, { pid::volume, 0.85f }, { pid::pan, 0.f },
        { pid::reverbSend, 1.f }, { pid::chorusSend, 1.f }, { pid::delaySend, 1.f },
        { pid::attack, 0.f }, { pid::decay, 0.f }, { pid::sustain, 1.f }, { pid::release, 0.12f },
        { pid::cutoff, 20000.f }, { pid::resonance, 0.15f }, { pid::filterType, 0.f },
        { pid::revDry, 1.f }, { pid::revWet, 0.30f }, { pid::revTime, 0.55f }, { pid::revDamp, 0.5f },
        { pid::choDry, 1.f }, { pid::choWet, 0.25f }, { pid::choRate, 0.3f }, { pid::choDepth, 0.35f },
        { pid::dlyDry, 1.f }, { pid::dlyWet, 0.f }, { pid::dlyTime, 0.35f }, { pid::dlyFeedback, 0.35f },
        { pid::polyphony, 2.f }, { pid::glide, 0.25f }, { pid::glideOn, 0.f },
        { pid::transpose, 0.f }, { pid::octave, 0.f }, { pid::tune, 0.f } };

    // Nota: "ZAY PIANO ORGAN" usa por ahora las mismas muestras del SF2 de fabrica (B3 Leslie)
    // con otros ajustes; cuando exista un SF2 de piano, solo hay que cambiar sf2Path/bank/program.
    ZayPreset po = organ;
    po.name = "ZAY PIANO ORGAN";
    po.values = V {
        { pid::master, 0.8f }, { pid::volume, 0.85f }, { pid::pan, 0.f },
        { pid::reverbSend, 1.f }, { pid::chorusSend, 1.f }, { pid::delaySend, 1.f },
        { pid::attack, 0.05f }, { pid::decay, 0.45f }, { pid::sustain, 0.55f }, { pid::release, 0.30f },
        { pid::cutoff, 8000.f }, { pid::resonance, 0.2f }, { pid::filterType, 0.f },
        { pid::revDry, 1.f }, { pid::revWet, 0.45f }, { pid::revTime, 0.7f }, { pid::revDamp, 0.45f },
        { pid::choDry, 1.f }, { pid::choWet, 0.12f }, { pid::choRate, 0.25f }, { pid::choDepth, 0.3f },
        { pid::dlyDry, 1.f }, { pid::dlyWet, 0.1f }, { pid::dlyTime, 0.4f }, { pid::dlyFeedback, 0.3f },
        { pid::polyphony, 2.f }, { pid::glide, 0.25f }, { pid::glideOn, 0.f },
        { pid::transpose, 0.f }, { pid::octave, 0.f }, { pid::tune, 0.f } };

    presets.clear();
    presets.push_back(organ);
    presets.push_back(po);
    numFactory = (int) presets.size();
}

void PresetManager::refreshUserPresets()
{
    presets.resize((size_t) numFactory);
    auto folder = getPresetFolder();
    if (! folder.isDirectory()) return;

    for (auto& f : folder.findChildFiles(juce::File::findFiles, false, "*.zkpreset"))
    {
        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(f));
        if (xml == nullptr || ! xml->hasTagName("ZAYPRESET")) continue;
        ZayPreset p;
        p.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension());
        p.sf2Path = xml->getStringAttribute("sf2", kFactoryPathStr);
        p.bank = xml->getIntAttribute("bank", 0);
        p.program = xml->getIntAttribute("program", 0);
        for (auto* e : xml->getChildWithTagNameIterator("P"))
            p.values.emplace_back(e->getStringAttribute("id"), (float) e->getDoubleAttribute("v"));
        presets.push_back(std::move(p));
    }
}

int PresetManager::saveUser(const juce::String& nameIn, juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& sf2Path, int bank, int program)
{
    auto name = nameIn.trim();
    if (name.isEmpty()) return -1;
    for (int i = 0; i < numFactory; ++i)
        if (presets[(size_t) i].name.equalsIgnoreCase(name)) { name << " (user)"; break; }

    auto folder = getPresetFolder();
    if (! folder.createDirectory().wasOk()) return -1;

    juce::XmlElement xml("ZAYPRESET");
    xml.setAttribute("name", name);
    xml.setAttribute("sf2", sf2Path);
    xml.setAttribute("bank", bank);
    xml.setAttribute("program", program);
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p))
        {
            auto* e = xml.createNewChildElement("P");
            e->setAttribute("id", r->paramID);
            e->setAttribute("v", (double) r->convertFrom0to1(r->getValue()));
        }

    auto file = folder.getChildFile(juce::File::createLegalFileName(name) + ".zkpreset");
    if (! xml.writeTo(file)) return -1;

    refreshUserPresets();
    for (int i = numFactory; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == name) return i;
    return -1;
}

void PresetManager::applyValues(juce::AudioProcessorValueTreeState& apvts, const ZayPreset& p)
{
    for (auto& kv : p.values)
        if (auto* prm = apvts.getParameter(kv.first))
            prm->setValueNotifyingHost(prm->convertTo0to1(kv.second));
}
} // namespace zk

#include "LCDDisplay.h"
#include "CustomLookAndFeel.h"

namespace zk
{
namespace
{
struct ParamRef { const char* id; const char* label; };
const ParamRef kParams[] = {
    { "volume", "VOLUME" }, { "pan", "PAN" }, { "master", "MASTER" },
    { "reverbSend", "REV SEND" }, { "chorusSend", "CHO SEND" }, { "delaySend", "DLY SEND" },
    { "attack", "ATTACK" }, { "decay", "DECAY" }, { "sustain", "SUSTAIN" }, { "release", "RELEASE" },
    { "cutoff", "CUTOFF" }, { "resonance", "RESONANCE" }, { "filterType", "FILTER TYPE" },
    { "glide", "GLIDE" }, { "glideOn", "GLIDE SW" }, { "polyphony", "POLYPHONY" },
    { "transpose", "TRANSPOSE" }, { "octave", "OCTAVE" }, { "tune", "FINE TUNE" } };
constexpr int kNumParams = (int) (sizeof(kParams) / sizeof(kParams[0]));
}

LCDDisplay::LCDDisplay(ZayKeysProcessor& p) : proc(p)
{
    proc.addChangeListener(this);
    syncCursor();
    startTimerHz(8);   // refresca contador de voces
}

LCDDisplay::~LCDDisplay() { proc.removeChangeListener(this); }

int LCDDisplay::lineCount() const
{
    switch (page)
    {
        case Presets:     return (int) proc.presets.getAll().size();
        case Instruments: return proc.getFontInfo() ? (int) proc.getFontInfo()->entries.size() : 0;
        case Params:      return kNumParams;
        default:          return 5;
    }
}

int LCDDisplay::activeIndex() const
{
    if (page == Presets) return proc.getPresetIndex();
    if (page == Instruments && proc.getFontInfo())
    {
        auto& v = proc.getFontInfo()->entries;
        for (int i = 0; i < (int) v.size(); ++i)
            if (v[(size_t) i].bank == proc.getBank() && v[(size_t) i].program == proc.getProgram()) return i;
    }
    return -1;
}

void LCDDisplay::syncCursor()
{
    if (page == Presets || page == Instruments)
    {
        const int a = activeIndex();
        if (a >= 0) cursor = a;
    }
    cursor = juce::jlimit(0, juce::jmax(0, lineCount() - 1), cursor);
}

LCDDisplay::Line LCDDisplay::getLine(int i) const
{
    Line l;
    auto info = proc.getFontInfo();
    switch (page)
    {
        case Presets:
        {
            auto& all = proc.presets.getAll();
            l.left = all[(size_t) i].name;
            l.right = all[(size_t) i].factory ? "F" : "U";
            l.active = (i == proc.getPresetIndex());
            break;
        }
        case Instruments:
        {
            const auto& e = info->entries[(size_t) i];
            l.left = juce::String(e.bank).paddedLeft('0', 3) + ":" + juce::String(e.program).paddedLeft('0', 3) + " " + e.name;
            l.active = (e.bank == proc.getBank() && e.program == proc.getProgram());
            break;
        }
        case Params:
        {
            l.left = kParams[i].label;
            if (auto* p = proc.apvts.getParameter(kParams[i].id))
                l.right = p->getCurrentValueAsText() + (p->getLabel().isEmpty() ? "" : " " + p->getLabel());
            break;
        }
        default:
        {
            const char* st[] = { "EMPTY", "LOADING", "READY", "ERROR" };
            juce::String instr = "-";
            if (info)
                for (auto& e : info->entries)
                    if (e.bank == proc.getBank() && e.program == proc.getProgram()) instr = e.name;
            switch (i)
            {
                case 0: l.left = "SF2: " + (info ? info->name : juce::String("-")); break;
                case 1: l.left = "STATE: " + juce::String(st[(int) proc.getLoadState()]); break;
                case 2: l.left = "BANK " + juce::String(proc.getBank()).paddedLeft('0', 3) + " PRG " + juce::String(proc.getProgram()).paddedLeft('0', 3); break;
                case 3: l.left = "INSTR: " + instr; break;
                default: l.left = "VOICES " + juce::String(proc.getVoiceCount()) + "/" + juce::String(proc.getVoiceLimit())
                                + "  KEYS " + juce::String(proc.getKeyMin()) + "-" + juce::String(proc.getKeyMax()); break;
            }
        }
    }
    return l;
}

void LCDDisplay::setPage(int p) { page = p; cursor = 0; syncCursor(); repaint(); }

void LCDDisplay::scroll(int d)
{
    cursor = juce::jlimit(0, juce::jmax(0, lineCount() - 1), cursor + d);
    repaint();
}

void LCDDisplay::enter()
{
    if (cursor >= lineCount()) return;
    if (page == Presets) proc.loadPreset(cursor);
    else if (page == Instruments)
    {
        const auto& e = proc.getFontInfo()->entries[(size_t) cursor];
        proc.selectInstrument(e.bank, e.program);
    }
    repaint();
}

void LCDDisplay::turnValue(int steps)
{
    if (page != Params) { scroll(-steps); return; }
    if (auto* p = proc.apvts.getParameter(kParams[cursor].id))
    {
        const int n = p->getNumSteps();
        const float step = (n > 1 && n < 1000) ? 1.0f / (float) (n - 1) : 0.02f;
        p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + (float) steps * step));
    }
    repaint();
}

void LCDDisplay::pickRow(int row)
{
    cursor = juce::jlimit(0, juce::jmax(0, lineCount() - 1), cursor + row);
    enter();
}

void LCDDisplay::softKey(int idx)
{
    if (idx < numPages) setPage(idx);
    else if (idx == 4 && onLoadSoundFont) onLoadSoundFont();
    else if (idx == 5 && onSavePreset) onSavePreset();
}

void LCDDisplay::paint(juce::Graphics& g)
{
    const float s = (float) getHeight() / 148.0f;
    const auto b = getLocalBounds().toFloat();
    const juce::Colour ink(0xff081a52), paper(0xff6a9be0);

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff7fb0f2), 0.f, 0.f, juce::Colour(0xff5b8fd8), 0.f, b.getBottom(), false));
    g.fillRect(b);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    for (float y = 0; y < b.getHeight(); y += 3.0f) g.drawHorizontalLine((int) y, 0.f, b.getRight());

    // titulo / estado
    juce::String title = "ZAY KEYS PRO v1.0.1";
    if (proc.getLoadState() == LoadState::Loading) title = "LOADING SOUNDFONT...";
    else if (proc.getLoadState() == LoadState::Error) title = proc.getStatusText();
    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions("Consolas", 15.0f * s, juce::Font::bold)));
    g.drawText(title, juce::Rectangle<float>(4.f * s, 0.f, b.getWidth() - 8.f * s, 24.f * s).toNearestInt(),
               juce::Justification::centred, true);

    // lista
    const float rowH = 20.0f * s;
    const int n = lineCount();
    const juce::String pageName[] = { "PRESET", "INSTR", "PARAM", "INFO" };
    for (int r = 0; r < numRows; ++r)
    {
        const int idx = (page == Info ? r : cursor + r);
        if (idx >= n) break;
        const auto line = getLine(idx);
        const auto row = juce::Rectangle<float>(3.f * s, 24.f * s + (float) r * rowH, b.getWidth() - 6.f * s, rowH);
        const bool hi = (r == 0 && page != Info);
        if (hi) { g.setColour(ink); g.fillRect(row); }
        g.setColour(hi ? juce::Colour(0xffd8e8ff) : ink);
        g.setFont(juce::Font(juce::FontOptions("Consolas", 16.0f * s, juce::Font::bold)));
        const juce::String prefix = hi ? pageName[page] + ":" : (line.active ? "* " : ". ");
        g.drawText(prefix + line.left, row.reduced(3.f * s, 0.f).toNearestInt(), juce::Justification::centredLeft, true);
        if (line.right.isNotEmpty())
            g.drawText(line.right, row.reduced(3.f * s, 0.f).toNearestInt(), juce::Justification::centredRight, true);
    }

    // teclas de funcion
    const char* labels[6] = { "PRESET", "INSTR", "PARAM", "INFO", "LOAD", "SAVE" };
    const float cw = b.getWidth() / 6.0f, sy = b.getBottom() - 22.0f * s;
    for (int i = 0; i < 6; ++i)
    {
        auto cell = juce::Rectangle<float>((float) i * cw, sy, cw, 22.0f * s);
        const bool sel = (i == page);
        g.setColour(sel ? ink : paper.withAlpha(0.0f)); g.fillRect(cell);
        g.setColour(ink); g.drawRect(cell, 1.0f);
        g.setColour(sel ? juce::Colour(0xffd8e8ff) : ink);
        g.setFont(juce::Font(juce::FontOptions("Consolas", 11.0f * s, juce::Font::bold)));
        g.drawText(labels[i], cell.toNearestInt(), juce::Justification::centred, false);
    }
}
} // namespace zk

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "PluginProcessor.h"

namespace zk
{
// Pantalla LCD azul: 4 paginas (PRESET, INSTR, PARAM, INFO) con datos reales del plugin.
class LCDDisplay : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    enum Page { Presets, Instruments, Params, Info, numPages };
    static constexpr int numRows = 5;

    explicit LCDDisplay(ZayKeysProcessor&);
    ~LCDDisplay() override;
    void paint(juce::Graphics&) override;

    void setPage(int p);
    void movePage(int d) { setPage((page + d + numPages) % numPages); }
    void scroll(int d);
    void enter();
    void turnValue(int steps);       // rueda de datos
    void pickRow(int row);           // botones laterales izquierdos
    void softKey(int idx);           // botones inferiores

    std::function<void()> onLoadSoundFont, onSavePreset;

private:
    struct Line { juce::String left, right; bool active = false; };
    int  lineCount() const;
    Line getLine(int i) const;
    int  activeIndex() const;
    void syncCursor();
    void changeListenerCallback(juce::ChangeBroadcaster*) override { syncCursor(); repaint(); }
    void timerCallback() override { repaint(); }

    ZayKeysProcessor& proc;
    int page = Presets, cursor = 0;
};
} // namespace zk

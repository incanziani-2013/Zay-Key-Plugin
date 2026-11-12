#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace zk
{
// Teclado de 88 teclas (A0..C8). Clic/arrastre -> notas; muestra notas activas (MIDI entrante incluido).
class KeyboardComponent : public juce::Component, private juce::Timer
{
public:
    explicit KeyboardComponent(ZayKeysProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    static constexpr int firstNote = 21, lastNote = 108;

private:
    static bool isBlack(int note) { const int pc = note % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    int noteAt(juce::Point<float> p, float& velocity) const;
    bool isActive(int note) const;
    void timerCallback() override;
    void release();

    ZayKeysProcessor& proc;
    juce::Rectangle<float> rects[lastNote - firstNote + 1];
    int mouseNote = -1;
    uint64_t lastMask[2] = { 0, 0 };
};
} // namespace zk

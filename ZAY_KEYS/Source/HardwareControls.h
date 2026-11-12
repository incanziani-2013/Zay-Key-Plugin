#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace zk
{
// Boton fisico (gris, con LED opcional). Se usa tambien para pintar botones del panel.
class HwButton : public juce::Button
{
public:
    enum class Style { Rect, Red };
    explicit HwButton(const juce::String& name, Style s = Style::Rect, bool led = false);
    void paintButton(juce::Graphics&, bool over, bool down) override;
    void setSymbol(const juce::String& s) { symbol = s; repaint(); }
    static void paintFace(juce::Graphics&, juce::Rectangle<float> r, Style s, bool pressed, bool lit, bool led,
                          const juce::String& symbol = {});
private:
    Style style; bool showLed; juce::String symbol;
};

// Rueda de datos: giro por arrastre / rueda del mouse -> pasos enteros.
class DataWheel : public juce::Component
{
public:
    std::function<void(int)> onTurn;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void turn(int steps);
    juce::Point<int> last; float acc = 0.f, angle = 0.f;
};

// Display de 7 segmentos (indice de banco).
class SevenSegment : public juce::Component
{
public:
    void setValue(int v, bool dot);
    void paint(juce::Graphics&) override;
private:
    int value = 0; bool dp = false;
};

// Medidor de nivel estereo (LEDs).
class LevelMeter : public juce::Component
{
public:
    void setLevels(float l, float r);
    void paint(juce::Graphics&) override;
private:
    float lv[2] = { 0.f, 0.f };
};
} // namespace zk

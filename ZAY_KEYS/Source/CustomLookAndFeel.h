#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace zk
{
inline juce::Font uiFont(float h, bool bold = true)
{
    return juce::Font(juce::FontOptions("Arial", h, bold ? juce::Font::bold : juce::Font::plain));
}

// Perillas negras y sliders verticales con apariencia fisica.
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                          float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                          float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override { return 9; }
};
} // namespace zk

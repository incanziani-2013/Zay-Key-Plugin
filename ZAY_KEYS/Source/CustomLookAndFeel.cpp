#include "CustomLookAndFeel.h"

namespace zk
{
CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffb0b0b0));
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    const float d = (float) juce::jmin(w, h) - 4.0f;
    const auto c = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = d * 0.5f;

    // marcas de escala impresas en el panel
    g.setColour(juce::Colour(0xff2a2a2a));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const float r1 = r + 2.5f, r2 = r + (i % 5 == 0 ? 6.0f : 4.5f);
        g.drawLine(c.x + std::sin(a) * r1, c.y - std::cos(a) * r1, c.x + std::sin(a) * r2, c.y - std::cos(a) * r2, 1.0f);
    }

    // sombra
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(c.x - r + 1.5f, c.y - r + 3.0f, d, d);
    // cuerpo
    juce::ColourGradient body(juce::Colour(0xff5a5a5a), c.x - r * 0.4f, c.y - r * 0.6f,
                              juce::Colour(0xff050505), c.x + r * 0.6f, c.y + r * 0.9f, true);
    g.setGradientFill(body);
    g.fillEllipse(c.x - r, c.y - r, d, d);
    // aro
    g.setColour(juce::Colour(0xff1a1a1a));
    g.drawEllipse(c.x - r, c.y - r, d, d, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawEllipse(c.x - r * 0.72f, c.y - r * 0.72f, d * 0.72f, d * 0.72f, 1.0f);
    // indicador
    const float a = startAngle + pos * (endAngle - startAngle);
    g.setColour(juce::Colours::white);
    g.drawLine(c.x + std::sin(a) * r * 0.35f, c.y - std::cos(a) * r * 0.35f,
               c.x + std::sin(a) * r * 0.88f, c.y - std::cos(a) * r * 0.88f, 2.2f);
}

void CustomLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cx = (float) x + (float) w * 0.5f;
    // ranura
    auto slot = juce::Rectangle<float>(cx - 3.0f, (float) y + 4.0f, 6.0f, (float) h - 8.0f);
    g.setColour(juce::Colour(0xff0c0c0c));
    g.fillRoundedRectangle(slot, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawRoundedRectangle(slot.expanded(0.8f), 2.5f, 0.8f);
    // marcas
    g.setColour(juce::Colour(0xff303030));
    for (int i = 0; i <= 5; ++i)
    {
        const float ty = slot.getY() + slot.getHeight() * (float) i / 5.0f;
        g.drawLine(cx - 12.0f, ty, cx - 8.0f, ty, 1.0f);
    }
    // tapa
    auto cap = juce::Rectangle<float>(cx - 12.0f, sliderPos - 8.0f, 24.0f, 16.0f);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(cap.translated(0.0f, 2.0f), 2.5f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4a4a), 0.0f, cap.getY(),
                                           juce::Colour(0xff161616), 0.0f, cap.getBottom(), false));
    g.fillRoundedRectangle(cap, 2.5f);
    g.setColour(juce::Colour(0xff777777));
    g.drawRoundedRectangle(cap, 2.5f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawLine(cap.getX() + 3.0f, cap.getCentreY(), cap.getRight() - 3.0f, cap.getCentreY(), 1.5f);
}
} // namespace zk

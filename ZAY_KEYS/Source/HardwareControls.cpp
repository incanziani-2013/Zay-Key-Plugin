#include "HardwareControls.h"
#include "CustomLookAndFeel.h"
#include <cmath>

namespace zk
{
HwButton::HwButton(const juce::String& name, Style s, bool led) : juce::Button(name), style(s), showLed(led) {}

void HwButton::paintFace(juce::Graphics& g, juce::Rectangle<float> b, Style st, bool pressed, bool lit, bool led,
                         const juce::String& sym)
{
    if (led)
    {
        const auto c = juce::Point<float>(b.getCentreX(), b.getY() + 2.5f);
        g.setColour(lit ? juce::Colour(0xffff3b30) : juce::Colour(0xff4a0d0d));
        g.fillEllipse(c.x - 2.2f, c.y - 2.2f, 4.4f, 4.4f);
        if (lit) { g.setColour(juce::Colour(0x55ff3b30)); g.fillEllipse(c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f); }
        b = b.withTrimmedTop(6.0f);
    }
    b = b.reduced(1.0f);
    if (pressed) b = b.translated(0.0f, 1.0f);

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(b.translated(0.0f, 1.5f), 3.0f);

    const juce::Colour top = st == Style::Red ? (lit || pressed ? juce::Colour(0xffff4a3a) : juce::Colour(0xffe02a22))
                                              : juce::Colour(0xffe8e8e8);
    const juce::Colour bot = st == Style::Red ? juce::Colour(0xff8f120d) : juce::Colour(0xff9a9a9a);
    g.setGradientFill(pressed ? juce::ColourGradient(bot, 0.f, b.getY(), top, 0.f, b.getBottom(), false)
                              : juce::ColourGradient(top, 0.f, b.getY(), bot, 0.f, b.getBottom(), false));
    g.fillRoundedRectangle(b, 3.0f);
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(b, 3.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawLine(b.getX() + 3.0f, b.getY() + 1.5f, b.getRight() - 3.0f, b.getY() + 1.5f, 1.0f);

    if (sym.isNotEmpty())
    {
        g.setColour(juce::Colour(0xff202020));
        g.setFont(uiFont(juce::jmin(13.0f, b.getHeight() * 0.7f)));
        g.drawText(sym, b.toNearestInt(), juce::Justification::centred, false);
    }
}

void HwButton::paintButton(juce::Graphics& g, bool, bool down)
{
    paintFace(g, getLocalBounds().toFloat(), style, down, getToggleState(), showLed, symbol);
}

// ------------------------------------------------------------------ DataWheel
void DataWheel::turn(int steps)
{
    angle += (float) steps * 0.3f;
    if (onTurn) onTurn(steps);
    repaint();
}

void DataWheel::mouseDown(const juce::MouseEvent& e) { last = e.getPosition(); acc = 0.f; }

void DataWheel::mouseDrag(const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    acc += (float) (last.y - p.y) + (float) (p.x - last.x);
    last = p;
    while (acc >= 6.f)  { acc -= 6.f; turn(1); }
    while (acc <= -6.f) { acc += 6.f; turn(-1); }
}

void DataWheel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (w.deltaY != 0.f) turn(w.deltaY > 0.f ? 1 : -1);
}

void DataWheel::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(3.0f);
    const auto c = b.getCentre();
    const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(c.x - r + 2.0f, c.y - r + 4.0f, r * 2.f, r * 2.f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3a3a), c.x - r * 0.5f, c.y - r * 0.7f,
                                           juce::Colour(0xff030303), c.x + r * 0.7f, c.y + r, true));
    g.fillEllipse(c.x - r, c.y - r, r * 2.f, r * 2.f);
    g.setColour(juce::Colour(0xff5a5a5a));
    g.drawEllipse(c.x - r, c.y - r, r * 2.f, r * 2.f, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = angle + (float) i * juce::MathConstants<float>::twoPi / 36.0f;
        g.drawLine(c.x + std::sin(a) * r * 0.80f, c.y - std::cos(a) * r * 0.80f,
                   c.x + std::sin(a) * r * 0.96f, c.y - std::cos(a) * r * 0.96f, 1.2f);
    }
    g.setColour(juce::Colour(0xff6a6a6a));
    g.fillEllipse(c.x + std::sin(angle) * r * 0.58f - 4.f, c.y - std::cos(angle) * r * 0.58f - 4.f, 8.f, 8.f);
    g.setColour(juce::Colour(0xff111111));
    g.fillEllipse(c.x - r * 0.28f, c.y - r * 0.28f, r * 0.56f, r * 0.56f);
}

// ------------------------------------------------------------------ SevenSegment
void SevenSegment::setValue(int v, bool dot)
{
    v = juce::jlimit(0, 9, v);
    if (v != value || dot != dp) { value = v; dp = dot; repaint(); }
}

void SevenSegment::paint(juce::Graphics& g)
{
    static const int seg[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff06101a)); g.fillRoundedRectangle(b, 3.0f);
    g.setColour(juce::Colour(0xff5a5a5a)); g.drawRoundedRectangle(b, 3.0f, 1.5f);
    const auto in = b.reduced(b.getWidth() * 0.26f, b.getHeight() * 0.14f);
    const float t = juce::jmax(2.5f, in.getWidth() * 0.18f), hw = in.getWidth() - t, hh = (in.getHeight() - t) * 0.5f;
    const float x0 = in.getX(), y0 = in.getY();
    const juce::Rectangle<float> r[7] = {
        { x0 + t * 0.5f, y0, hw, t }, { x0 + hw, y0 + t * 0.5f, t, hh }, { x0 + hw, y0 + hh + t * 0.5f, t, hh },
        { x0 + t * 0.5f, y0 + 2 * hh, hw, t }, { x0, y0 + hh + t * 0.5f, t, hh }, { x0, y0 + t * 0.5f, t, hh },
        { x0 + t * 0.5f, y0 + hh, hw, t } };
    for (int i = 0; i < 7; ++i)
    {
        const bool on = (seg[value] >> i) & 1;
        g.setColour(on ? juce::Colour(0xff35e8ff) : juce::Colour(0xff0e2a34));
        g.fillRect(r[i]);
    }
    if (dp) { g.setColour(juce::Colour(0xff35e8ff)); g.fillEllipse(b.getRight() - 7.f, b.getBottom() - 8.f, 3.f, 3.f); }
}

// ------------------------------------------------------------------ LevelMeter
void LevelMeter::setLevels(float l, float r)
{
    const float a = juce::jlimit(0.f, 1.f, l), b = juce::jlimit(0.f, 1.f, r);
    if (std::abs(a - lv[0]) > 0.004f || std::abs(b - lv[1]) > 0.004f) { lv[0] = a; lv[1] = b; repaint(); }
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0a0a0a)); g.fillRoundedRectangle(b, 2.0f);
    const int segs = 14;
    const float sh = (b.getHeight() - 4.0f) / (float) segs;
    const float bw = (b.getWidth() - 6.0f) * 0.5f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float db = juce::Decibels::gainToDecibels(lv[ch], -60.0f);
        const int lit = juce::roundToInt(juce::jmap(db, -60.0f, 0.0f, 0.0f, (float) segs));
        for (int i = 0; i < segs; ++i)
        {
            const bool on = i < lit;
            const juce::Colour col = i >= segs - 2 ? juce::Colour(0xffff3030) : (i >= segs - 5 ? juce::Colour(0xffffd030) : juce::Colour(0xff30e060));
            g.setColour(on ? col : col.withAlpha(0.14f));
            g.fillRect(2.0f + (float) ch * (bw + 2.0f), b.getBottom() - 2.0f - (float) (i + 1) * sh + 0.8f, bw, sh - 1.2f);
        }
    }
}
} // namespace zk

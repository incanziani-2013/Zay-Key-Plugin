#include "KeyboardComponent.h"

namespace zk
{
KeyboardComponent::KeyboardComponent(ZayKeysProcessor& p) : proc(p) { startTimerHz(30); }

void KeyboardComponent::resized()
{
    int whites = 0;
    for (int n = firstNote; n <= lastNote; ++n) if (! isBlack(n)) ++whites;
    const float ww = (float) getWidth() / (float) whites, h = (float) getHeight();
    const float bw = ww * 0.6f, bh = h * 0.62f;

    int wi = 0;
    for (int n = firstNote; n <= lastNote; ++n)
    {
        auto& r = rects[n - firstNote];
        if (! isBlack(n)) { r = { (float) wi * ww, 0.f, ww, h }; ++wi; }
        else
        {
            float shift = 0.f;
            switch (n % 12) { case 1: shift = -0.12f; break; case 3: shift = 0.12f; break;
                              case 6: shift = -0.16f; break; case 8: shift = 0.f; break; default: shift = 0.16f; }
            r = { (float) wi * ww - bw * 0.5f + shift * bw, 0.f, bw, bh };
        }
    }
}

bool KeyboardComponent::isActive(int note) const
{
    const int bit = note - firstNote;
    if (note == mouseNote) return true;
    return ((proc.getActiveMask(bit / 64) >> (bit % 64)) & 1) != 0;
}

void KeyboardComponent::timerCallback()
{
    const uint64_t a = proc.getActiveMask(0), b = proc.getActiveMask(1);
    if (a != lastMask[0] || b != lastMask[1]) { lastMask[0] = a; lastMask[1] = b; repaint(); }
}

void KeyboardComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
    for (int n = firstNote; n <= lastNote; ++n)
    {
        if (isBlack(n)) continue;
        auto r = rects[n - firstNote].reduced(0.5f, 0.0f);
        const bool on = isActive(n);
        g.setGradientFill(on ? juce::ColourGradient(juce::Colour(0xffb8cdf0), 0.f, r.getY(), juce::Colour(0xff8fa9d6), 0.f, r.getBottom(), false)
                             : juce::ColourGradient(juce::Colour(0xfffcfcfc), 0.f, r.getY(), juce::Colour(0xffdedede), 0.f, r.getBottom(), false));
        g.fillRect(r.withTrimmedBottom(on ? 3.0f : 0.0f));
        g.setColour(juce::Colour(0xff7a7a7a));
        g.drawRect(r, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRect(r.getX() + 1.0f, r.getBottom() - 6.0f, r.getWidth() - 2.0f, 4.0f);
    }
    for (int n = firstNote; n <= lastNote; ++n)
    {
        if (! isBlack(n)) continue;
        auto r = rects[n - firstNote];
        const bool on = isActive(n);
        if (on) r = r.withTrimmedBottom(3.0f);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(r.translated(2.5f, 1.5f), 2.0f);
        g.setGradientFill(on ? juce::ColourGradient(juce::Colour(0xff3d5f95), 0.f, r.getY(), juce::Colour(0xff0f1c33), 0.f, r.getBottom(), false)
                             : juce::ColourGradient(juce::Colour(0xff3c3c3c), 0.f, r.getY(), juce::Colour(0xff030303), 0.f, r.getBottom(), false));
        g.fillRoundedRectangle(r, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.14f));
        g.fillRect(r.getX() + 2.0f, r.getY() + 1.0f, r.getWidth() - 4.0f, r.getHeight() * 0.55f);
        g.setColour(juce::Colour(0xff000000));
        g.drawRoundedRectangle(r, 2.0f, 1.0f);
    }
}

int KeyboardComponent::noteAt(juce::Point<float> p, float& velocity) const
{
    int found = -1;
    for (int n = firstNote; n <= lastNote && found < 0; ++n)
        if (isBlack(n) && rects[n - firstNote].contains(p)) found = n;
    for (int n = firstNote; n <= lastNote && found < 0; ++n)
        if (! isBlack(n) && rects[n - firstNote].contains(p)) found = n;
    if (found >= 0)
    {
        const auto& r = rects[found - firstNote];
        velocity = juce::jmap(juce::jlimit(0.0f, 1.0f, (p.y - r.getY()) / r.getHeight()), 0.0f, 1.0f, 0.45f, 1.0f);
    }
    return found;
}

void KeyboardComponent::release()
{
    if (mouseNote >= 0) proc.pushUiNote(false, mouseNote, 0.f);
    mouseNote = -1;
}

void KeyboardComponent::mouseDown(const juce::MouseEvent& e)
{
    float v = 0.8f;
    const int n = noteAt(e.position, v);
    if (n >= 0) { mouseNote = n; proc.pushUiNote(true, n, v); }
    repaint();
}

void KeyboardComponent::mouseDrag(const juce::MouseEvent& e)
{
    float v = 0.8f;
    const int n = noteAt(e.position, v);
    if (n != mouseNote)
    {
        release();
        if (n >= 0) { mouseNote = n; proc.pushUiNote(true, n, v); }
        repaint();
    }
}

void KeyboardComponent::mouseUp(const juce::MouseEvent&) { release(); repaint(); }
} // namespace zk

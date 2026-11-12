#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "HardwareControls.h"
#include "KeyboardComponent.h"
#include "LCDDisplay.h"

// Interfaz disenada en un espacio de 1376x768 (la imagen de referencia) y escalada a cualquier tamano.
class ZayKeysEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ZayKeysEditor(ZayKeysProcessor&);
    ~ZayKeysEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override {}

private:
    struct Txt { juce::String text; float x, y, size; };
    struct Panel { float x, y, w, h; juce::String title; };
    struct Inert { float x, y, w, h; bool red; };

    class Background : public juce::Component
    {
    public:
        Background() { setBufferedToImage(true); setInterceptsMouseClicks(false, false); }
        void paint(juce::Graphics&) override;
        float scale = 1.0f;
        std::vector<Txt> texts;
        std::vector<Panel> panels;
        std::vector<Inert> inerts;
    };

    struct Placed { juce::Component* c; float cx, cy, w, h; };

    void timerCallback() override;
    void knob(juce::Slider& s, const juce::String& id, float cx, float cy, float size);
    void vslider(juce::Slider& s, const juce::String& id, float cx, float cy);
    HwButton& button(HwButton& b, float cx, float cy, float w, float h, const juce::String& symbol,
                     std::function<void()> fn);
    void place(juce::Component& c, float cx, float cy, float w, float h);
    void text(const juce::String& t, float x, float y, float size = 11.0f) { bg.texts.push_back({ t, x, y, size }); }
    void askSaveName();
    void chooseSoundFont();

    ZayKeysProcessor& proc;
    zk::CustomLookAndFeel lnf;
    Background bg;
    std::vector<Placed> placed;

    juce::Slider sMaster, sPan, sRevDry, sRevWet, sChoDry, sChoWet, sDlyDry, sDlyWet,
        sAttack, sDecay, sSustain, sRelease, sCutoff, sResonance, sGlide;
    juce::Slider vRevSend, vChoSend, vVolume, vDlySend, vRevTime, vRevDamp, vChoRate, vChoDepth, vDlyTime, vDlyFb;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;

    // botones del panel
    std::vector<std::unique_ptr<zk::HwButton>> buttons;
    zk::HwButton glideSwitch { "glideSwitch", zk::HwButton::Style::Red, true };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> glideAtt;
    zk::HwButton poly[4] { zk::HwButton("p16"), zk::HwButton("p32"), zk::HwButton("p64"), zk::HwButton("p128") };
    std::unique_ptr<juce::ParameterAttachment> polyAtt;

    zk::LCDDisplay lcd;
    zk::DataWheel wheel;
    zk::SevenSegment seg;
    zk::LevelMeter meter;
    zk::KeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZayKeysEditor)
};

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg        { 0xff0c0c0c };
    const juce::Colour body      { 0xff141414 };
    const juce::Colour gold      { 0xfff2c230 };
    const juce::Colour goldLight { 0xffffe48a };
    const juce::Colour goldDark  { 0xffa87c0a };
    const juce::Colour goldDeep  { 0xff5c4304 };
    const juce::Colour text      { 0xfff3ead2 };
    const juce::Colour textDim   { 0xff9c8a5a };
    const juce::Colour well      { 0xff050505 };
    const juce::Colour red       { 0xffff3b30 };

    juce::Font font (float size, bool bold = false);
    juce::String t (const char* utf8);                   // UTF-8 text helper (Turkish characters)
    void sunken (juce::Graphics&, juce::Rectangle<float>); // XP-style inset box
}

class APLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override;
};

// Windows XP style push button (square, small radius, glossy), toggles.
class XPButton : public juce::Button
{
public:
    XPButton (const juce::String& label, bool led);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    bool hasLed;
};

// Windows XP style check box with a label.
class XPCheckBox : public juce::Button
{
public:
    explicit XPCheckBox (const juce::String& label) : juce::Button (label) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class WaveformView : public juce::Component
{
public:
    void push (const WaveBin& b);
    void paint (juce::Graphics&) override;
private:
    static constexpr int kBins = 420;
    std::array<WaveBin, kBins> bins {};
    int write = 0;
};

class HelpOverlay : public juce::Component
{
public:
    std::function<void()> onClose;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClose) onClose(); }
};

class APDeClickEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit APDeClickEditor (APDeClickProcessor&);
    ~APDeClickEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateSize();
    juce::String statusText() const;

    APDeClickProcessor& proc;
    APLookAndFeel lnf;

    juce::Slider sensitivity, frequency, widen;
    XPButton listenBtn, bypassBtn;
    XPCheckBox waveBox;
    XPButton helpBtn;
    WaveformView wave;
    HelpOverlay help;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> sensA, freqA, widenA;
    std::unique_ptr<BA> listenA, bypassA;

    juce::Rectangle<int> groupArea;
    std::array<WaveBin, 512> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (APDeClickEditor)
};

#pragma once

#include "PluginProcessor.h"

namespace MoosColours
{
    const juce::Colour bg       { 0xff12160f };
    const juce::Colour panel    { 0xff1c2318 };
    const juce::Colour outline  { 0xff2e3a27 };
    const juce::Colour moss     { 0xff9ccc5a };
    const juce::Colour lichen   { 0xffd8e6b0 };
    const juce::Colour dim      { 0xff6f7d62 };
}

class MoosLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MoosLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class WavetableView : public juce::Component, private juce::Timer
{
public:
    WavetableView (MoosProcessor& p);
    ~WavetableView() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    MoosProcessor& proc;
};

class MoosEditor : public juce::AudioProcessorEditor
{
public:
    explicit MoosEditor (MoosProcessor&);
    ~MoosEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        std::vector<Knob*> knobs;
        juce::Rectangle<int> bounds;
    };

    Knob* addKnob (const juce::String& paramId, const juce::String& text);
    void layoutSection (Section&, juce::Rectangle<int>);

    MoosProcessor& proc;
    MoosLookAndFeel lnf;
    WavetableView tableView;
    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoosEditor)
};

#pragma once

#include "PluginProcessor.h"
#include "MossBackground.h"

namespace MoosColours
{
    const juce::Colour bg       { 0xff0d120b };
    const juce::Colour glass    { 0x880b1009 };   // halbtransparente Panel-Fläche
    const juce::Colour outline  { 0x40c6e07a };
    const juce::Colour moss     { 0xffa8d65c };
    const juce::Colour lichen   { 0xffe4eec8 };
    const juce::Colour dim      { 0xff9aa98a };
}

class MoosLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MoosLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override        { return juce::FontOptions (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (12.5f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override                       { return juce::FontOptions (14.0f); }
};

//==============================================================================
class WavetableView : public juce::Component, private juce::Timer
{
public:
    WavetableView (MoosProcessor& p, int oscIndex);
    ~WavetableView() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    MoosProcessor& proc;
    int osc;
};

//==============================================================================
class ModMatrixComponent : public juce::Component
{
public:
    explicit ModMatrixComponent (MoosProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Row
    {
        juce::ComboBox src, dst;
        juce::Slider amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt, dstAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAtt;
    };
    Row rows[MoosParams::numModSlots];
};

//==============================================================================
class MoosEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MoosEditor (MoosProcessor&);
    ~MoosEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1200, baseH = 780;
    void showPage (int page);

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Item
    {
        juce::Component* comp = nullptr;
        juce::Label* label = nullptr;
        float weight = 1.0f;
        int kind = 0;   // 0 = Drehregler, 1 = Auswahlliste/Schalter, 2 = frei
    };

    struct Section
    {
        juce::String title;
        int page = 0;
        std::vector<Item> items;
        std::vector<juce::Component*> headerButtons;
        juce::Rectangle<int> bounds;
    };

    // Erzeugung
    Item knob (const juce::String& id, const juce::String& text);
    Item combo (const juce::String& id, const juce::String& text);
    Item toggle (const juce::String& id, const juce::String& text);
    Item custom (juce::Component& c, float weight);
    Section& addSection (const juce::String& title, int page, std::vector<Item> items);
    juce::Label* makeLabel (const juce::String& text);

    void layoutSection (Section&, juce::Rectangle<int>);
    void timerCallback() override;

    // Presets
    void refreshPresetList();
    void presetSelected();
    void stepPreset (int delta);
    void savePreset();

    // Wavetables & Hintergrund
    void chooseWavetable (int osc);
    void backgroundMenu();
    void loadBackground();

    MoosProcessor& proc;
    MoosLookAndFeel lnf;

    // Alle Bedienelemente liegen in "content" (fest 1200x780) und werden als Ganzes skaliert
    struct Content : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    } content;
    void paintContent (juce::Graphics&);
    void layoutContent();

    juce::Image bgImage, bgBlur;
    juce::SharedResourcePointer<MossBackground::MossCache> mossCache;
    bool waitingForMoss = false;

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAtts;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;
    std::vector<Section> sections;

    WavetableView viewA, viewB;
    ModMatrixComponent matrix;
    juce::Label matrixHint;

    juce::TextButton pageButtons[3];
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, savePresetButton { "SPEICHERN" }, bgButton { "HINTERGRUND" };
    juce::TextButton loadA { "WAV" }, resetA { "X" }, loadB { "WAV" }, resetB { "X" };
    juce::ComboBox presetBox;
    juce::Array<juce::File> userPresetFiles;
    juce::MidiKeyboardComponent keyboard;

    std::unique_ptr<juce::FileChooser> chooser;
    int currentPage = 0;
    int lastPresetCounter = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoosEditor)
};

#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// TUSCHE – Optik nach Christophs Tuschezeichnungen: handgeschöpftes Papier, trockener Pinsel,
// Druckblöcke, Zellkreise, Bleistift, ein einziger grüner Akzent.
namespace ink
{
    const juce::Colour paper    { 0xffefede7 };
    const juce::Colour ink      { 0xff151515 };
    const juce::Colour graphite { 0xff5c5c5c };
    const juce::Colour green    { 0xff2fae4e };

    // Rechteck mit ausgefransten Kanten (wie ein Druckstock)
    inline juce::Path roughRect (juce::Rectangle<float> r, juce::Random& rnd, float jitter)
    {
        juce::Path p;
        const int steps = 14;
        auto edge = [&] (juce::Point<float> a, juce::Point<float> b, bool first)
        {
            for (int i = first ? 0 : 1; i <= steps; ++i)
            {
                const float t = (float) i / steps;
                auto q = a + (b - a) * t;
                q += { (rnd.nextFloat() - 0.5f) * jitter, (rnd.nextFloat() - 0.5f) * jitter };
                if (first && i == 0) p.startNewSubPath (q); else p.lineTo (q);
            }
        };
        edge (r.getTopLeft(), r.getTopRight(), true);
        edge (r.getTopRight(), r.getBottomRight(), false);
        edge (r.getBottomRight(), r.getBottomLeft(), false);
        edge (r.getBottomLeft(), r.getTopLeft(), false);
        p.closeSubPath();
        return p;
    }

    // trockener Pinsel: mehrere versetzte Striche, dazu Papier-Aussparungen
    inline void dryStroke (juce::Graphics& g, const juce::Path& p, float width, juce::Colour c, juce::Random& rnd, float dryness = 0.5f)
    {
        g.setColour (c.withMultipliedAlpha (0.85f));
        g.strokePath (p, juce::PathStrokeType (width * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (int k = 0; k < 6; ++k)
        {
            const float dx = (rnd.nextFloat() - 0.5f) * width * 0.7f, dy = (rnd.nextFloat() - 0.5f) * width * 0.7f;
            g.setColour (c.withMultipliedAlpha (0.18f + 0.2f * rnd.nextFloat()));
            g.strokePath (p, juce::PathStrokeType (width * (0.2f + 0.3f * rnd.nextFloat()), juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          juce::AffineTransform::translation (dx, dy));
        }
        // Aussparungen entlang des Strichs
        const float len = p.getLength();
        g.setColour (paper.withAlpha (0.55f * dryness));
        for (float d = 0.0f; d < len; d += 2.5f + rnd.nextFloat() * 6.0f)
        {
            auto q = p.getPointAlongPath (d);
            const float s = 0.6f + rnd.nextFloat() * width * 0.22f;
            g.fillEllipse (q.x + (rnd.nextFloat() - 0.5f) * width * 0.8f, q.y + (rnd.nextFloat() - 0.5f) * width * 0.8f, s * 1.6f, s);
        }
    }

    // gefüllter Druckblock mit trockener Textur
    inline void stamp (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, juce::Random& rnd, float speckle = 1.0f)
    {
        auto p = roughRect (r, rnd, 2.2f);
        g.setColour (c);
        g.fillPath (p);
        g.setColour (paper.withAlpha (0.5f));
        const int n = (int) (r.getWidth() * r.getHeight() / 60.0f * speckle);
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
            const float s = 0.5f + rnd.nextFloat() * 1.4f;
            g.fillEllipse (x, y, s, s * (0.5f + rnd.nextFloat()));
        }
    }

    inline void pencilText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, float size = 11.0f,
                            juce::Colour c = graphite)
    {
        g.setColour (c);
        g.setFont (juce::FontOptions (size).withStyle ("Italic"));
        juce::String spaced;
        for (auto ch : t) { spaced << juce::String::charToString (ch); if (ch != ' ') spaced << juce::String::charToString (0x200A); }
        g.drawText (spaced, r, j);
    }
}

//==============================================================================
class InkLookAndFeel : public tmk::LookAndFeel
{
public:
    explicit InkLookAndFeel (const tmk::Theme& t) : tmk::LookAndFeel (t)
    {
        using namespace ink;
        setColour (juce::Slider::textBoxTextColourId, ink::ink);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, ink::ink);
        setColour (juce::ComboBox::textColourId, ink::ink);
        setColour (juce::ComboBox::arrowColourId, ink::ink);
        setColour (juce::PopupMenu::backgroundColourId, paper);
        setColour (juce::PopupMenu::textColourId, ink::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ink::ink);
        setColour (juce::PopupMenu::highlightedTextColourId, paper);
        setColour (juce::PopupMenu::headerTextColourId, graphite);
        setColour (juce::TextButton::textColourOffId, ink::ink);
        setColour (juce::TextButton::textColourOnId, paper);
        setColour (juce::TextEditor::backgroundColourId, paper);
        setColour (juce::TextEditor::textColourId, ink::ink);
        setColour (juce::TextEditor::outlineColourId, graphite);
        setColour (juce::AlertWindow::backgroundColourId, paper);
        setColour (juce::AlertWindow::textColourId, ink::ink);
    }

    // Zellkreis-Drehregler (nach „Photobiota“): trockener Tuschering, helles Inneres mit Pünktchen,
    // ein ausgesparter weißer Fleck zeigt den Wert, innen ein Pinselbogen
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override
    {
        using namespace ink;
        juce::Random rnd ((juce::int64) (reinterpret_cast<juce::pointer_sized_int> (&s) & 0xffffff));
        const float r = juce::jmin (w, h) * 0.5f - 3.0f;
        const juce::Point<float> c (x + w * 0.5f, y + h * 0.5f);
        const float t = r * 0.27f;
        // Kern
        g.setColour (ink::ink.withAlpha (0.05f));
        g.fillEllipse (juce::Rectangle<float> ((r - t) * 2.0f, (r - t) * 2.0f).withCentre (c));
        // Ring aus vielen trockenen Strichen
        for (int k = 0; k < 12; ++k)
        {
            juce::Path ring;
            const float rad = r - t * (0.1f + 0.85f * rnd.nextFloat());
            const float a0 = rnd.nextFloat() * 6.28f;
            for (int i = 0; i <= 48; ++i)
            {
                const float a = a0 + juce::MathConstants<float>::twoPi * i / 48.0f * (0.85f + 0.15f * rnd.nextFloat());
                const float rr = rad + (rnd.nextFloat() - 0.5f) * t * 0.25f;
                const juce::Point<float> q (c.x + std::cos (a) * rr, c.y + std::sin (a) * rr);
                if (i == 0) ring.startNewSubPath (q); else ring.lineTo (q);
            }
            g.setColour (ink::ink.withAlpha (0.35f + 0.5f * rnd.nextFloat()));
            g.strokePath (ring, juce::PathStrokeType (t * (0.25f + 0.35f * rnd.nextFloat()), juce::PathStrokeType::curved));
        }
        // Pünktchen im Kern
        g.setColour (ink::ink.withAlpha (0.55f));
        for (int i = 0; i < 6; ++i)
        {
            const float a = rnd.nextFloat() * 6.28f, d = rnd.nextFloat() * (r - t) * 0.45f;
            const float sz = 1.0f + rnd.nextFloat() * 2.2f;
            g.fillEllipse (c.x + std::cos (a) * d - sz * 0.5f, c.y + std::sin (a) * d - sz * 0.5f, sz, sz * 1.3f);
        }
        // Wertebogen innen
        const float angle = start + pos * (end - start);
        if (pos > 0.002f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r - t - 5.0f, r - t - 5.0f, 0.0f, start, angle, true);
            dryStroke (g, arc, 3.6f, s.isMouseButtonDown() ? green : ink::ink, rnd, 0.6f);
        }
        // ausgesparter Fleck auf dem Ring = Zeiger
        const juce::Point<float> e (c.x + std::sin (angle) * (r - t * 0.5f), c.y - std::cos (angle) * (r - t * 0.5f));
        juce::Path eye;
        eye.addEllipse (-t * 0.38f, -t * 0.62f, t * 0.76f, t * 1.24f);
        eye.applyTransform (juce::AffineTransform::rotation (angle).translated (e));
        g.setColour (paper);
        g.fillPath (eye);
        if (s.isMouseOverOrDragging())
        {
            g.setColour (green.withAlpha (0.8f));
            g.strokePath (eye, juce::PathStrokeType (1.2f));
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace ink;
        juce::Random rnd ((juce::int64) (reinterpret_cast<juce::pointer_sized_int> (&b) & 0xffff));
        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        if (b.getToggleState()) stamp (g, r, down ? ink::ink.brighter (0.2f) : ink::ink, rnd, 0.6f);
        else
        {
            auto p = roughRect (r, rnd, 1.6f);
            g.setColour (ink::ink.withAlpha (over ? 0.08f : 0.03f));
            g.fillPath (p);
            g.setColour (graphite.withAlpha (down ? 0.9f : 0.6f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
        }
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace ink;
        juce::Random rnd ((juce::int64) (reinterpret_cast<juce::pointer_sized_int> (&box) & 0xffff));
        juce::Path line;
        line.startNewSubPath (3.0f, h - 3.0f);
        for (int i = 1; i <= 8; ++i) line.lineTo (3.0f + (w - 6.0f) * i / 8.0f, h - 3.0f + (rnd.nextFloat() - 0.5f) * 1.4f);
        g.setColour (graphite.withAlpha (box.isMouseOver() ? 1.0f : 0.7f));
        g.strokePath (line, juce::PathStrokeType (1.1f));
        juce::Path tri;
        tri.addTriangle ((float) w - 15.0f, h * 0.4f, (float) w - 7.0f, h * 0.4f, (float) w - 11.0f, h * 0.62f);
        g.setColour (ink::ink);
        g.fillPath (tri);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 0, box.getWidth() - 18, box.getHeight() - 2);
        label.setFont (juce::FontOptions (13.0f));
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::textColourId, ink::ink);
        l->setColour (juce::Label::outlineWhenEditingColourId, ink::green);
        l->setFont (juce::FontOptions (12.5f).withStyle ("Italic"));
        return l;
    }

    void drawLabel (juce::Graphics& g, juce::Label& l) override
    {
        if (l.isBeingEdited()) return;
        g.setColour (l.findColour (juce::Label::textColourId));
        g.setFont (getLabelFont (l));
        g.drawFittedText (l.getText(), l.getLocalBounds(), l.getJustificationType(), 1, 0.9f);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int, int) override
    {
        g.fillAll (ink::paper);
        g.setColour (ink::graphite.withAlpha (0.5f));
        g.drawRect (g.getClipBounds(), 1);
    }
};

//==============================================================================
// Die große Anzeige: ein Ausschnitt aus einem deiner Blätter, darauf die Wellenform in Papierweiß,
// die 16 Striche als Bleistiftmarken und die Körner als Pinseltupfer.
class InkDisplay : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    InkDisplay (TuscheProcessor& p);
    ~InkDisplay() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; }
    void filesDropped (const juce::StringArray&, int, int) override;
    std::function<void (float)> onPositionDrag;     // vom Editor: Position des gewählten Strichs setzen
    std::function<void (bool)> onGesture;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> waveArea() const { return getLocalBounds().toFloat().reduced (14.0f, 30.0f).withTrimmedBottom (10.0f); }
    juce::Rectangle<float> blattButton() const { return { 12.0f, 6.0f, 120.0f, 20.0f }; }
    juce::Rectangle<float> loadButton() const { return { (float) getWidth() - 132.0f, 6.0f, 120.0f, 20.0f }; }
    void rebuildPeaks();
    TuscheProcessor& proc;
    std::vector<float> peaks;
    int peaksFor = -1, peaksWidth = 0;
    const void* peaksSource = nullptr;
    bool dragOver = false, draggingPos = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

// 16 Druckblöcke: Klick wählt, Umschalt-Klick schaltet an/aus, Rechtsklick: Kopieren/Einfügen/…
class StepRow : public juce::Component, private juce::Timer
{
public:
    explicit StepRow (TuscheProcessor& p) : proc (p) { startTimerHz (30); }
    ~StepRow() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> cell (int i) const;
    int selected() const { return juce::jlimit (0, tusche::numChops - 1, (int) proc.apvts.getRawParameterValue ("auswahl")->load() - 1); }
    void setParam (const juce::String& id, float plain);
    TuscheProcessor& proc;
    static inline float clip[tusche::numChopParams + 1] {};
    static inline bool hasClip = false;
};

// Regler für den gewählten Strich, mit Verknüpfung „alle“
class ChopKnob : public juce::Component
{
public:
    ChopKnob (TuscheProcessor& p, int paramIndex, const juce::String& caption);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh (int chop);
    bool linked() const;
    juce::String format (float normalised) const;
private:
    juce::RangedAudioParameter* param (int chop) const { return proc.apvts.getParameter (tusche::cid (chop, tusche::chopParams[k])); }
    juce::Rectangle<int> linkArea() const { return { getWidth() / 2 - 22, getHeight() - 16, 44, 15 }; }
    TuscheProcessor& proc;
    const int k;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::String caption;
    int chop = 0;
    bool syncing = false;
};

// Hüllkurve als Pinselstrich
class EnvView : public juce::Component, private juce::Timer
{
public:
    explicit EnvView (TuscheProcessor& p) : proc (p) { startTimerHz (15); }
    ~EnvView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    TuscheProcessor& proc;
};

//==============================================================================
class TuscheEditor : public tmk::Editor
{
public:
    explicit TuscheEditor (TuscheProcessor& p);
    ~TuscheEditor() override;

private:
    void layoutBody (juce::Rectangle<int> body) override;
    bool paintSection (juce::Graphics&, juce::Rectangle<float>, const juce::String&) override;
    bool paintKeyboardFrame (juce::Graphics&, juce::Rectangle<float>) override;
    void paintHeader (juce::Graphics&, juce::Rectangle<float>) override;
    void tick() override;
    static tmk::Editor::Config makeConfig (TuscheProcessor&);

    TuscheProcessor& tp;
    InkLookAndFeel inkLnf;
    InkDisplay display;
    StepRow steps;
    EnvView env;
    std::vector<std::unique_ptr<ChopKnob>> chopKnobs;
    int shownChop = -1;
};

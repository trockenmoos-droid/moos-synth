#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Sechs konzentrische Euklid-Ringe: jeder Punkt ein Schritt, gefüllt = Schlag, glühend = gerade gespielt
class ForgeRings : public juce::Component, private juce::Timer
{
public:
    ForgeRings (HammerProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (30); }
    ~ForgeRings() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x55000000));
        g.fillRoundedRectangle (r, 6.0f);
        auto ring = r.reduced (10.0f).withTrimmedTop (16.0f);
        const float side = juce::jmin (ring.getWidth(), ring.getHeight());
        const auto c = ring.withSizeKeepingCentre (side, side).getCentre();
        const float rMax = side * 0.47f, rMin = side * 0.12f;

        // Glut in der Mitte (stärker beim Stottern)
        const float glow = proc.stuttering.load() ? 1.0f : 0.35f;
        juce::ColourGradient cg (juce::Colour (0xffff7a1a).withAlpha (0.45f * glow), c.x, c.y,
                                 juce::Colour (0x00ff7a1a), c.x + rMin * 1.8f, c.y, true);
        g.setGradientFill (cg); g.fillEllipse (juce::Rectangle<float> (rMin * 3.6f, rMin * 3.6f).withCentre (c));

        for (int t = 0; t < hammer::numTracks; ++t)
        {
            const float rad = rMax - t * (rMax - rMin) / (hammer::numTracks - 1);
            const int st = juce::jlimit (2, 32, (int) param (t, "sch"));
            const int hc = juce::jlimit (0, st, (int) param (t, "hit") + proc.hitOffset[t].load());
            const int ro = (int) param (t, "dreh") + proc.rotOffset[t].load();
            const int cur = proc.curStep[t].load();
            const float flash = proc.hitFlash[t].load();
            const float lvl = param (t, "lvl");
            g.setColour (theme.outline.withAlpha (0.35f));
            g.drawEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c), 1.0f);
            const float dot = juce::jlimit (3.0f, 9.0f, rad * 6.28f / st * 0.32f);
            for (int s = 0; s < st; ++s)
            {
                const float a = juce::MathConstants<float>::twoPi * s / st - juce::MathConstants<float>::halfPi;
                const juce::Point<float> p (c.x + std::cos (a) * rad, c.y + std::sin (a) * rad);
                const bool on = hammer::euclid (s, st, hc, ro);
                juce::Colour col = on ? theme.accent.withAlpha (0.4f + 0.6f * lvl) : juce::Colour (0x30ffffff);
                float d = on ? dot : dot * 0.55f;
                if (s == cur)
                {
                    col = on ? juce::Colour (0xffffe0a0).interpolatedWith (theme.accent, 1.0f - flash) : juce::Colour (0x90ffffff);
                    d *= on ? 1.0f + 0.6f * flash : 1.3f;
                    if (on && flash > 0.1f)
                    {
                        g.setColour (juce::Colour (0xffff8a20).withAlpha (0.35f * flash));
                        g.fillEllipse (juce::Rectangle<float> (d * 3.0f, d * 3.0f).withCentre (p));
                    }
                }
                g.setColour (col);
                g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
            }
        }
        g.setFont (juce::FontOptions (11.0f));
        for (int t = 0; t < hammer::numTracks; ++t)
        {
            g.setColour (theme.accent.withAlpha (0.3f + 0.7f * proc.hitFlash[t].load()));
            g.drawText (juce::String::fromUTF8 (hammer::trackNames[t]), juce::Rectangle<float> (r.getX() + 12.0f, r.getBottom() - 16.0f - (hammer::numTracks - t) * 14.0f, 90.0f, 14.0f),
                        juce::Justification::centredLeft);
        }
        g.setColour (theme.dim);
        g.drawText (juce::String::fromUTF8 ("au\xc3\x9f" "en \xe2\x86\x92 innen"), r.reduced (12.0f, 6.0f), juce::Justification::bottomLeft);
    }

private:
    float param (int t, const char* p) const { return proc.apvts.getRawParameterValue (hammer::pid (t, p))->load(); }
    void timerCallback() override { repaint(); }
    HammerProcessor& proc;
    tmk::Theme theme;
};


// Quelle einer Spur: Klick schaltet Synth -> Sample -> Beide
class SourceButton : public juce::Component
{
public:
    SourceButton (HammerProcessor& p, int track, const tmk::Theme& t)
        : theme (t), param (*p.apvts.getParameter (hammer::pid (track, "quelle"))),
          att (param, [this] (float) { repaint(); }, nullptr)
    {
        att.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paint (juce::Graphics& g) override
    {
        const int v = index();
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth() - 4.0f, 26.0f);
        g.setColour (v == 0 ? juce::Colour (0x40000000) : theme.accent.withAlpha (v == 1 ? 0.85f : 0.45f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (v == 0 ? theme.outline : theme.accent);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (v == 1 ? juce::Colour (0xff160c04) : theme.text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (hammer::sources[v], r, juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        att.setValueAsCompleteGesture ((float) ((index() + 1) % 3));
    }
private:
    int index() const { return juce::jlimit (0, 2, juce::roundToInt (param.convertFrom0to1 (param.getValue()))); }
    tmk::Theme theme;
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment att;
};

// Sample-Feld einer Spur: Klick öffnet Menü, Audiodateien oder Ordner können hineingezogen werden
class SampleSlot : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    SampleSlot (HammerProcessor& p, int track, const tmk::Theme& t) : proc (p), tr (track), theme (t)
    {
        startTimerHz (4);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    ~SampleSlot() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const auto* sd = proc.sampleFor (tr);
        g.setColour (dragOver ? theme.accent.withAlpha (0.35f) : juce::Colour (0x40000000));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (sd != nullptr || dragOver ? theme.accent : theme.outline.withAlpha (0.8f));
        if (sd != nullptr || dragOver) g.drawRoundedRectangle (r, 5.0f, 1.4f);
        else
        {
            juce::Path p; p.addRoundedRectangle (r, 5.0f);
            juce::Path d; const float dash[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (d, p, dash, 2);
            g.fillPath (d);
        }
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.setColour (theme.dim);
        g.drawText ("SAMPLE", r.removeFromTop (16.0f), juce::Justification::centred);
        g.setFont (juce::FontOptions (11.0f));
        g.setColour (sd != nullptr ? theme.text : theme.dim.withAlpha (0.7f));
        g.drawFittedText (sd != nullptr ? sd->name : juce::String::fromUTF8 ("hierher ziehen\noder klicken"),
                          r.reduced (3.0f, 1.0f).toNearestInt(), juce::Justification::centred, 3, 0.8f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        juce::PopupMenu m;
        m.addItem (1, juce::String::fromUTF8 ("Sample laden\xe2\x80\xa6"));
        m.addItem (2, juce::String::fromUTF8 ("Kit laden (Ordner \xe2\x86\x92 alle 6 Spuren)\xe2\x80\xa6"));
        m.addItem (3, "Sample entfernen", proc.sampleFor (tr) != nullptr);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int id)
        {
            if (id == 1 || id == 2)
            {
                const auto start = proc.sampleFor (tr) != nullptr ? proc.sampleFor (tr)->file.getParentDirectory()
                                                                   : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
                chooser = std::make_unique<juce::FileChooser> (id == 1 ? "Sample laden" : "Kit-Ordner oder eine Datei daraus", start,
                                                               id == 1 ? "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3" : "*");
                const int flags = juce::FileBrowserComponent::openMode
                                | (id == 1 ? juce::FileBrowserComponent::canSelectFiles
                                           : juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectFiles);
                chooser->launchAsync (flags, [this, id] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (f == juce::File()) return;
                    if (id == 1) proc.loadSample (tr, f);
                    else proc.loadKit (f, 0);
                    repaint();
                });
            }
            else if (id == 3) { proc.clearSample (tr); repaint(); }
        });
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (HammerProcessor::isAudioFile (juce::File (f)) || juce::File (f).isDirectory()) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        if (files.size() == 1 && juce::File (files[0]).isDirectory()) proc.loadKit (juce::File (files[0]), tr);
        else
        {
            // mehrere Dateien: diese Spur und die folgenden
            int t = tr;
            for (auto& f : files)
                if (t < hammer::numTracks && proc.loadSample (t, juce::File (f)).isEmpty()) ++t;
        }
        repaint();
    }

private:
    void timerCallback() override
    {
        const int c = proc.sampleChanges.load();
        if (c != seen) { seen = c; repaint(); }
    }
    HammerProcessor& proc;
    const int tr;
    tmk::Theme theme;
    bool dragOver = false;
    int seen = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

class HammerEditor : public tmk::Editor
{
public:
    explicit HammerEditor (HammerProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig();
    ForgeRings rings;
    std::vector<std::unique_ptr<SampleSlot>> slots;
    std::vector<std::unique_ptr<SourceButton>> sourceButtons;
};

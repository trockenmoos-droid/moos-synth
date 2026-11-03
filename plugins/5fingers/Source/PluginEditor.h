#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Die Plattform: fünf Finger ragen über den Abgrund, Länge = Verzögerung, Helligkeit = Pegel,
// Funken an der Spitze = aktive Körner der Granular-Wolke
class CliffView : public juce::Component, private juce::Timer
{
public:
    CliffView (FingersProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (40); }
    ~CliffView() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x40000000));
        g.fillRoundedRectangle (r, 6.0f);

        auto v = [&] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const float baseMs = proc.baseMs.load();

        // Felskante links
        const juce::Point<float> anchor (r.getX() + 90.0f, r.getCentreY() + 4.0f);
        juce::Path cliff;
        cliff.startNewSubPath (r.getX(), r.getY() + 30.0f);
        cliff.lineTo (anchor.x - 30.0f, r.getY() + 40.0f);
        cliff.lineTo (anchor.x + 4.0f, anchor.y - 20.0f);
        cliff.lineTo (anchor.x + 8.0f, anchor.y + 6.0f);
        cliff.lineTo (anchor.x - 10.0f, r.getBottom());
        cliff.lineTo (r.getX(), r.getBottom());
        cliff.closeSubPath();
        g.setColour (juce::Colour (0xe01a1620));
        g.fillPath (cliff);

        const double now = juce::Time::getMillisecondCounterHiRes();
        const float maxLen = r.getRight() - anchor.x - 70.0f;
        for (int f = 0; f < fingers::numFingers; ++f)
        {
            const float z = v (fingers::id ("zeit", f));
            const float lvl = v (fingers::id ("pegel", f));
            const float korn = v (fingers::id ("korn", f));
            // Finger fächern auf, bleiben aber im Bild
            const float len = 40.0f + maxLen * z;
            const float fan = ((float) f - 2.0f) * r.getHeight() * 0.17f * (0.35f + 0.65f * len / (40.0f + maxLen));
            const juce::Point<float> tip (anchor.x + len, anchor.y + fan);

            // Plattform mit Geländer
            g.setColour (juce::Colour (0xff2b2630));
            g.drawLine ({ anchor, tip }, 9.0f);
            g.setColour (theme.accent.withAlpha (0.25f + 0.75f * lvl));
            g.drawLine ({ anchor, tip }, 4.0f);
            g.setColour (theme.text.withAlpha (0.35f));
            g.drawLine (anchor.x, anchor.y - 5.0f, tip.x, tip.y - 5.0f, 1.0f);

            // Echo-Impuls, der im Takt der Verzögerung über den Finger läuft
            const double period = juce::jmax (40.0, (double) (z * baseMs));
            const float u = (float) std::fmod (now / period, 1.0);
            const float a = juce::jlimit (0.15f, 1.0f, proc.fingerLevel[f].load() * 3.0f);
            const auto pulse = anchor + (tip - anchor) * u;
            g.setColour (theme.text.withAlpha (a));
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (pulse));

            // Spitze und Granular-Funken
            g.setColour (theme.accent);
            g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (tip));
            const int grains = proc.grainCount[f].load();
            juce::Random rr ((juce::int64) (now / 60.0) + f * 101);
            for (int k = 0; k < grains * 2 + (korn > 0.01f ? 1 : 0); ++k)
            {
                const auto sp = tip + juce::Point<float> ((rr.nextFloat() - 0.3f) * 34.0f, (rr.nextFloat() - 0.5f) * 26.0f);
                g.setColour (juce::Colours::white.withAlpha (0.4f + 0.5f * rr.nextFloat()));
                g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre (sp));
            }

            g.setColour (theme.text);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (juce::String (f + 1) + "  " + juce::String (juce::roundToInt (z * baseMs)) + " ms",
                        (int) tip.x + 10, (int) tip.y - 8, 90, 16, juce::Justification::centredLeft);
        }

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        juce::String info = juce::String::fromUTF8 ("L\xc3\xa4nge ") + juce::String (juce::roundToInt (baseMs)) + " ms";
        if (v ("sync") > 0.5f) info << "  (" << fingers::divisions[(int) v ("division")] << ")";
        g.drawText (info, r.reduced (12.0f, 6.0f), juce::Justification::topLeft);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String::fromUTF8 ("Krippenstein \xc2\xb7 2.108 m"), r.reduced (12.0f, 6.0f), juce::Justification::topRight);
    }

private:
    void timerCallback() override { repaint(); }
    FingersProcessor& proc;
    tmk::Theme theme;
};

class FingersEditor : public tmk::Editor
{
public:
    explicit FingersEditor (FingersProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig();
    CliffView view;
};

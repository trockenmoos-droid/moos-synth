#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Glocke (leuchtet beim Anschlag) und Orgelprospekt (Pfeifen je Register, Höhe = Fußtonlage)
class ChurchView : public juce::Component, private juce::Timer
{
public:
    ChurchView (WolfgangProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (30); }
    ~ChurchView() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x50000000));
        g.fillRoundedRectangle (r, 6.0f);
        auto v = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };

        // Glocke
        auto bellArea = r.removeFromLeft (r.getWidth() * 0.38f).reduced (16.0f, 26.0f);
        const float bl = juce::jlimit (0.0f, 1.0f, proc.bellLevel.load() * 4.0f);
        const float cx = bellArea.getCentreX(), top = bellArea.getY() + 8.0f, bot = bellArea.getBottom() - 6.0f;
        const float w = juce::jmin (bellArea.getWidth(), (bot - top) * 1.1f);
        juce::Path bell;
        bell.startNewSubPath (cx - w * 0.16f, top);
        bell.quadraticTo (cx - w * 0.22f, top + (bot - top) * 0.55f, cx - w * 0.38f, bot - 10.0f);
        bell.quadraticTo (cx - w * 0.5f, bot, cx - w * 0.48f, bot);
        bell.lineTo (cx + w * 0.48f, bot);
        bell.quadraticTo (cx + w * 0.5f, bot, cx + w * 0.38f, bot - 10.0f);
        bell.quadraticTo (cx + w * 0.22f, top + (bot - top) * 0.55f, cx + w * 0.16f, top);
        bell.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9a14a).withMultipliedBrightness (0.7f + 0.6f * bl), cx - w * 0.3f, top,
                                                 juce::Colour (0xff5a3c14), cx + w * 0.4f, bot, false));
        g.fillPath (bell);
        g.setColour (theme.accent.withAlpha (0.3f + 0.7f * bl));
        g.strokePath (bell, juce::PathStrokeType (1.5f + 3.0f * bl));
        g.fillRoundedRectangle (cx - 10.0f, top - 12.0f, 20.0f, 14.0f, 4.0f);
        // Schallwellen
        for (int k = 1; k <= 3; ++k)
        {
            g.setColour (theme.accent.withAlpha (bl * (0.5f - 0.13f * k)));
            g.drawEllipse (juce::Rectangle<float> (w * (0.6f + 0.25f * k), (bot - top) * (0.4f + 0.2f * k)).withCentre ({ cx, bot - 4.0f }), 1.5f);
        }

        // Orgelprospekt
        auto organArea = r.reduced (16.0f, 26.0f);
        const float ol = juce::jlimit (0.0f, 1.0f, proc.organLevel.load() * 5.0f);
        const float orgel = v ("orgel");
        const int perRank = 3;
        const float pw = organArea.getWidth() / (wolfgang::numRanks * perRank + 2);
        for (int rk = 0; rk < wolfgang::numRanks; ++rk)
        {
            const float amt = v (wolfgang::rankIds[rk]) * orgel;
            const float hFrac = 1.0f / std::sqrt (wolfgang::rankRatio[rk]);       // tiefe Register = lange Pfeifen
            for (int k = 0; k < perRank; ++k)
            {
                const float x = organArea.getX() + pw * (1 + rk * perRank + k);
                const float h = organArea.getHeight() * hFrac * (0.85f + 0.15f * (k == 1 ? 1.0f : 0.0f));
                auto pipe = juce::Rectangle<float> (x + pw * 0.15f, organArea.getBottom() - h, pw * 0.7f, h);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd6d8dc).withMultipliedBrightness (0.45f + 0.5f * amt + 0.3f * amt * ol), pipe.getX(), 0,
                                                         juce::Colour (0xff5d6066), pipe.getRight(), 0, false));
                g.fillRect (pipe);
                g.setColour (juce::Colour (0xff1c1c20));
                g.fillRect (juce::Rectangle<float> (pipe.getWidth() * 0.6f, 3.0f).withCentre ({ pipe.getCentreX(), organArea.getBottom() - h * 0.18f }));
            }
        }

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("GLOCKE  &  ORGEL", getLocalBounds().toFloat().reduced (12.0f, 6.0f), juce::Justification::topLeft);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String::fromUTF8 ("16'  8'  4'  2 2/3'  2'  1 3/5'  Mixtur"), getLocalBounds().toFloat().reduced (12.0f, 6.0f), juce::Justification::topRight);
    }

private:
    void timerCallback() override { repaint(); }
    WolfgangProcessor& proc;
    tmk::Theme theme;
};

class WolfgangEditor : public tmk::Editor
{
public:
    explicit WolfgangEditor (WolfgangProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig (WolfgangProcessor& p);
    ChurchView view;
};

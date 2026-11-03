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

class HammerEditor : public tmk::Editor
{
public:
    explicit HammerEditor (HammerProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig();
    ForgeRings rings;
};

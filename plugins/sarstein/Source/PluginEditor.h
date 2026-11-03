#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Kennlinie als Berggrat: Eingang (x) -> Ausgang (y), mit aktuellem Pegel
class TransferView : public juce::Component, private juce::Timer
{
public:
    TransferView (SarsteinProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (30); }
    ~TransferView() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x55000000));
        g.fillRoundedRectangle (r, 6.0f);

        auto v = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const int st = juce::jlimit (0, 3, (int) v ("stein"));
        const float drive = juce::Decibels::decibelsToGain (v ("drive"));
        const float bias = v ("geroell") * 0.6f;
        const float b0 = sarstein::shape (st, bias);
        const float crush = v ("steinschlag");
        const float steps = std::pow (2.0f, 15.0f - 13.0f * crush);

        auto area = r.reduced (14.0f, 24.0f).withTrimmedTop (6.0f);
        auto px = [&] (float x) { return area.getX() + (x + 1.0f) * 0.5f * area.getWidth(); };
        auto py = [&] (float y) { return area.getCentreY() - juce::jlimit (-1.2f, 1.2f, y) * area.getHeight() * 0.42f; };

        g.setColour (theme.outline.withAlpha (0.35f));
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
        g.drawVerticalLine ((int) area.getCentreX(), area.getY(), area.getBottom());
        g.drawLine (px (-1.0f), py (-1.0f), px (1.0f), py (1.0f), 1.0f);

        juce::Path curve, fill;
        for (int i = 0; i <= 300; ++i)
        {
            const float x = -1.0f + 2.0f * i / 300.0f;
            float y = sarstein::shape (st, x * drive + bias) - b0;
            if (crush > 0.001f) y = std::round (y * steps) / steps;
            if (i == 0) { curve.startNewSubPath (px (x), py (y)); fill.startNewSubPath (px (x), area.getBottom()); }
            curve.lineTo (px (x), py (y));
            fill.lineTo (px (x), py (y));
        }
        fill.lineTo (area.getRight(), area.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x90b7aea0), 0, area.getY(), juce::Colour (0x302a2620), 0, area.getBottom(), false));
        g.fillPath (fill);
        g.setColour (theme.accent);
        g.strokePath (curve, juce::PathStrokeType (2.5f));

        // aktueller Eingangspegel als senkrechte Markierung (beidseitig)
        const float in = juce::jlimit (0.0f, 1.0f, proc.inLevel.load());
        g.setColour (theme.text.withAlpha (0.6f));
        for (float sgn : { -1.0f, 1.0f }) g.drawVerticalLine ((int) px (sgn * in), area.getY(), area.getBottom());

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (sarstein::stones[st] + "  -  " + juce::String (v ("drive"), 1) + " dB", r.reduced (12.0f, 6.0f), juce::Justification::topLeft);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("Eingang  >  Ausgang", r.reduced (12.0f, 6.0f), juce::Justification::topRight);

        // Ausgangspegel
        const float lvl = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (proc.outLevel.load() + 1.0e-5f) + 48.0f) / 48.0f);
        auto meter = juce::Rectangle<float> (r.getRight() - 8.0f, area.getY(), 4.0f, area.getHeight());
        g.setColour (juce::Colour (0x40000000)); g.fillRect (meter);
        g.setColour (theme.accent); g.fillRect (meter.withTop (meter.getBottom() - meter.getHeight() * lvl));
    }

private:
    void timerCallback() override { repaint(); }
    SarsteinProcessor& proc;
    tmk::Theme theme;
};

class SarsteinEditor : public tmk::Editor
{
public:
    explicit SarsteinEditor (SarsteinProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig();
    TransferView view;
};

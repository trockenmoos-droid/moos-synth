#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Zeigt Predelay, frühe Reflexionen und den Nachhall (Mitten und Höhen) als Kurve
class DecayView : public juce::Component, private juce::Timer
{
public:
    DecayView (HallstattProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (30); }
    ~DecayView() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x60000000));
        g.fillRoundedRectangle (r, 6.0f);

        auto v = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const int room   = juce::jlimit (0, 4, (int) v ("room"));
        const float pre  = v ("predelay") / 1000.0f;
        const float dec  = v ("decay");
        const float damp = v ("damping");
        const float sz   = v ("size");
        const bool frz   = v ("freeze") > 0.5f;
        const float span = juce::jmax (1.0f, juce::jmin (32.0f, pre + dec * 1.15f + 0.2f));

        auto area = r.reduced (14.0f, 26.0f).withTrimmedBottom (4.0f);
        auto xOf = [&] (float t) { return area.getX() + area.getWidth() * t / span; };
        auto yOf = [&] (float db) { return area.getY() + area.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 60.0f); };

        // Raster
        g.setColour (theme.outline.withAlpha (0.25f));
        for (int db = 0; db >= -60; db -= 20) g.drawHorizontalLine ((int) yOf ((float) db), area.getX(), area.getRight());
        g.setFont (juce::FontOptions (10.0f));
        g.setColour (theme.dim);
        const float step = span > 12 ? 5.0f : (span > 4 ? 1.0f : 0.5f);
        for (float t = 0.0f; t <= span; t += step)
            g.drawText (juce::String (t, step < 1.0f ? 1 : 0) + " s", (int) xOf (t) - 20, (int) area.getBottom() + 2, 40, 14, juce::Justification::centred);

        // frühe Reflexionen
        const auto& rm = hallstatt::rooms()[(size_t) room];
        g.setColour (theme.text.withAlpha (0.7f));
        for (auto& tp : rm.taps)
        {
            const float t = pre + tp.ms * 0.001f * (0.5f + sz);
            if (t > span) continue;
            const float db = juce::Decibels::gainToDecibels (tp.gain * v ("early") + 1.0e-4f) - 6.0f;
            g.drawLine (xOf (t), yOf (-60.0f), xOf (t), yOf (db), 1.5f);
        }

        // Nachhallkurven: Mitten (Akzent) und Höhen (gedämpft)
        auto curve = [&] (float rt, juce::Colour c, float thick)
        {
            juce::Path p;
            p.startNewSubPath (xOf (0.0f), yOf (-60.0f));
            p.lineTo (xOf (pre), yOf (-60.0f));
            p.lineTo (xOf (pre + 0.02f), yOf (-3.0f));
            for (int i = 0; i <= 120; ++i)
            {
                const float t = pre + 0.02f + (span - pre) * i / 120.0f;
                const float db = frz ? -3.0f : -3.0f - 60.0f * (t - pre) / juce::jmax (0.05f, rt);
                p.lineTo (xOf (t), yOf (db));
            }
            g.setColour (c.withAlpha (0.18f));
            juce::Path fill (p);
            fill.lineTo (xOf (span), yOf (-60.0f));
            fill.closeSubPath();
            g.fillPath (fill);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (thick));
        };
        curve (dec * (1.0f - 0.85f * damp), theme.text.withAlpha (0.5f), 1.2f);
        curve (dec, theme.accent, 2.2f);

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (hallstatt::roomNames[room] + (frz ? "  -  FREEZE" : ""), r.reduced (12.0f, 6.0f), juce::Justification::topLeft);
        g.setFont (juce::FontOptions (11.0f));
        g.setColour (theme.dim);
        g.drawText (juce::String::fromUTF8 ("Mitten  /  H\xc3\xb6hen"), r.reduced (12.0f, 6.0f), juce::Justification::topRight);

        // Pegel des Halls
        const float lvl = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (proc.wetLevel.load() + 1.0e-5f) + 60.0f) / 60.0f);
        auto meter = juce::Rectangle<float> (r.getRight() - 8.0f, area.getY(), 4.0f, area.getHeight());
        g.setColour (juce::Colour (0x40000000));
        g.fillRect (meter);
        g.setColour (theme.accent);
        g.fillRect (meter.withTop (meter.getBottom() - meter.getHeight() * lvl));
    }

private:
    void timerCallback() override { repaint(); }
    HallstattProcessor& proc;
    tmk::Theme theme;
};

class HallstattEditor : public tmk::Editor
{
public:
    explicit HallstattEditor (HallstattProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig();
    DecayView view;
};

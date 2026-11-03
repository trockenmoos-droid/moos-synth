#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Spektrum als Eiszapfen: jeder Teilton hängt an seiner (gedehnten) Frequenz, Länge = Lautstärke
class IcicleView : public juce::Component, private juce::Timer
{
public:
    IcicleView (DachsteinProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (30); }
    ~IcicleView() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x40001020));
        g.fillRoundedRectangle (r, 6.0f);

        auto area = r.reduced (16.0f, 10.0f).withTrimmedTop (22.0f);
        const float maxRatio = 64.0f;
        auto xOf = [&] (float ratio) { return area.getX() + area.getWidth() * std::log (ratio) / std::log (maxRatio); };

        // Schneekante oben
        juce::Path snow;
        snow.startNewSubPath (area.getX() - 8.0f, area.getY());
        for (int i = 0; i <= 60; ++i)
        {
            const float x = area.getX() - 8.0f + (area.getWidth() + 16.0f) * i / 60.0f;
            snow.lineTo (x, area.getY() + 3.0f + 3.0f * std::sin (i * 1.7f) * std::sin (i * 0.37f));
        }
        snow.lineTo (area.getRight() + 8.0f, area.getY() - 8.0f);
        snow.lineTo (area.getX() - 8.0f, area.getY() - 8.0f);
        snow.closeSubPath();
        g.setColour (juce::Colour (0xfff0f8ff));
        g.fillPath (snow);

        const float act = proc.activity.load();
        for (int k = 0; k < dachstein::maxPartials; ++k)
        {
            const float a = proc.partialAmp[k].load();
            if (a <= 0.0001f) continue;
            const float ratio = proc.partialRatio[k].load();
            if (ratio > maxRatio) continue;
            const float db = juce::Decibels::gainToDecibels (a, -60.0f);
            const float len = area.getHeight() * juce::jlimit (0.04f, 1.0f, (db + 60.0f) / 60.0f);
            const float x = xOf (ratio);
            const float w = juce::jmax (3.0f, 10.0f - k * 0.2f);

            juce::Path ice;
            ice.startNewSubPath (x - w * 0.5f, area.getY());
            ice.quadraticTo (x - w * 0.35f, area.getY() + len * 0.6f, x, area.getY() + len);
            ice.quadraticTo (x + w * 0.35f, area.getY() + len * 0.6f, x + w * 0.5f, area.getY());
            ice.closeSubPath();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4fbff), x, area.getY(),
                                                     theme.accent.withAlpha (0.55f + 0.45f * act), x, area.getY() + len, false));
            g.fillPath (ice);
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawLine (x - w * 0.15f, area.getY() + 2.0f, x - w * 0.05f, area.getY() + len * 0.7f, 1.0f);
        }

        // Grundton-Raster (Harmonische) zum Vergleich: zeigt, wie "Kristall" die Teiltöne dehnt
        g.setColour (theme.text.withAlpha (0.18f));
        for (int h = 1; h <= 32; h *= 2)
            g.drawVerticalLine ((int) xOf ((float) h), area.getY() + 8.0f, area.getBottom());

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("GLETSCHER-SPEKTRUM"), r.reduced (12.0f, 6.0f), juce::Justification::topLeft);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String::fromUTF8 ("Linien = Oktaven \xc2\xb7 Zapfen = Teilt\xc3\xb6ne"), r.reduced (12.0f, 6.0f), juce::Justification::topRight);
    }

private:
    void timerCallback() override { repaint(); }
    DachsteinProcessor& proc;
    tmk::Theme theme;
};

class DachsteinEditor : public tmk::Editor
{
public:
    explicit DachsteinEditor (DachsteinProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig (DachsteinProcessor& p);
    IcicleView view;
};

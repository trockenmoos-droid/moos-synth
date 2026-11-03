#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Die Leitung: Rohr mit Stationen, Sole-Tropfen fließen im Takt der Verzögerung
class PipelineView : public juce::Component, private juce::Timer
{
public:
    PipelineView (SoleleitungProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (40); }
    ~PipelineView() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x50000000));
        g.fillRoundedRectangle (r, 6.0f);

        auto v = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const int nSt  = juce::jlimit (1, 4, (int) std::round (v ("stations")));
        const float fb = v ("feedback");
        const bool frz = v ("absperren") > 0.5f;
        const float ms = proc.delayMs.load();

        const float x0 = r.getX() + 70.0f, x1 = r.getRight() - 50.0f;
        auto yAt = [&] (float u) { return r.getY() + r.getHeight() * (0.42f + 0.12f * u) + 6.0f * std::sin (u * 7.0f); };   // leichtes Gefälle

        // Rohr
        juce::Path pipe;
        pipe.startNewSubPath (x0, yAt (0.0f));
        for (int i = 1; i <= 100; ++i) { const float u = i / 100.0f; pipe.lineTo (x0 + (x1 - x0) * u, yAt (u)); }
        g.setColour (juce::Colour (0xff2a1a0e));
        g.strokePath (pipe, juce::PathStrokeType (20.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff8a5a32));
        g.strokePath (pipe, juce::PathStrokeType (15.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0x50ffe2b0));
        g.strokePath (pipe, juce::PathStrokeType (4.0f));
        // Muffen alle paar Meter
        g.setColour (juce::Colour (0xff3a2512));
        for (int i = 1; i < 24; ++i)
        {
            const float u = i / 24.0f, x = x0 + (x1 - x0) * u;
            g.fillRect (juce::Rectangle<float> (3.0f, 22.0f).withCentre ({ x, yAt (u) }));
        }

        // Sole-Tropfen, die durch die Leitung fließen
        const double t = juce::Time::getMillisecondCounterHiRes();
        const float speed = (float) (t / juce::jmax (30.0, (double) ms));
        float level = 0.0f;
        for (int s = 0; s < 4; ++s) level = juce::jmax (level, proc.stationLevel[s].load());
        const float glow = juce::jlimit (0.15f, 1.0f, level * 3.0f);
        for (int j = 0; j < 12; ++j)
        {
            float u = std::fmod ((frz ? 0.0f : speed) + j / 12.0f, 1.0f);
            const float a = glow * (0.4f + 0.6f * std::pow (fb / 1.1f, 0.5f)) * (1.0f - 0.5f * u);
            g.setColour (juce::Colour (0xfff4f1e8).withAlpha (a));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x0 + (x1 - x0) * u, yAt (u) }));
        }

        // Quelle (Hallstatt) + Stationen
        auto station = [&] (float u, const juce::String& name, float lvl, bool active)
        {
            const auto c = juce::Point<float> (x0 + (x1 - x0) * u, yAt (u));
            const float rad = 11.0f + 8.0f * juce::jlimit (0.0f, 1.0f, lvl * 2.5f);
            if (active)
            {
                g.setColour (theme.accent.withAlpha (0.25f));
                g.fillEllipse (juce::Rectangle<float> (rad * 2.4f, rad * 2.4f).withCentre (c));
            }
            g.setColour (active ? theme.accent : theme.dim.withAlpha (0.6f));
            g.fillEllipse (juce::Rectangle<float> (rad, rad).withCentre (c));
            g.setColour (theme.text.withAlpha (active ? 1.0f : 0.5f));
            g.setFont (juce::FontOptions (12.0f, active ? juce::Font::bold : juce::Font::plain));
            g.drawText (name, (int) c.x - 60, (int) c.y + 18, 120, 16, juce::Justification::centred);
        };
        station (0.0f, "Hallstatt", 0.0f, false);
        for (int s = 1; s <= nSt; ++s)
        {
            const float u = (float) s / nSt;
            station (u, sole::stationName (s - 1, nSt), proc.stationLevel[s - 1].load(), true);
        }

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        juce::String info = juce::String (juce::roundToInt (ms)) + " ms";
        if (v ("sync") > 0.5f) info << "  (" << sole::divisions[(int) v ("division")] << ")";
        info << "   Feedback " << juce::roundToInt (fb * 100.0f) << " %";
        if (frz) info << "   -  ABGESPERRT";
        g.drawText (info, r.reduced (12.0f, 8.0f), juce::Justification::topLeft);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String::fromUTF8 ("Soleleitung seit 1595 \xc2\xb7 40 km Holzrohre"), r.reduced (12.0f, 8.0f), juce::Justification::topRight);
    }

private:
    void timerCallback() override { repaint(); }
    SoleleitungProcessor& proc;
    tmk::Theme theme;
};

class SoleleitungEditor : public tmk::Editor
{
public:
    explicit SoleleitungEditor (SoleleitungProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig();
    PipelineView view;
};

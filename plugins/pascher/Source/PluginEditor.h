#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Schrittraster: obere Reihe Gate, untere Reihe Klatschen. Klick schaltet um.
class StepGrid : public juce::Component, private juce::Timer
{
public:
    StepGrid (PascherProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (30); }
    ~StepGrid() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x55000000));
        g.fillRoundedRectangle (r, 6.0f);
        const int len = juce::jlimit (2, pascher::numSteps, (int) std::round (proc.apvts.getRawParameterValue ("laenge")->load()));
        const int cur = proc.currentStep.load();

        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        for (int row = 0; row < 2; ++row)
        {
            auto rowArea = rowBounds (row);
            g.setColour (theme.dim);
            g.drawText (row == 0 ? "GATE" : "KLATSCHEN", rowArea.withWidth (labelW).toNearestInt(), juce::Justification::centredLeft);
            for (int s = 0; s < pascher::numSteps; ++s)
            {
                auto cell = cellBounds (row, s);
                const bool on = value (row, s);
                const bool inRange = s < len;
                juce::Colour c = on ? theme.accent : juce::Colour (0x30ffffff);
                if (! inRange) c = c.withMultipliedAlpha (0.25f);
                if (s == cur && inRange) c = on ? theme.accent.brighter (0.6f) : juce::Colour (0x70ffffff);
                g.setColour (c);
                if (row == 1 && on)
                {
                    // Klatschen als kleine Hand-Symbol-Raute
                    juce::Path d; d.addStar (cell.getCentre(), 6, cell.getWidth() * 0.18f, cell.getWidth() * 0.4f);
                    g.fillPath (d);
                }
                else g.fillRoundedRectangle (cell, 4.0f);
                if (s % 4 == 0 || (len % 3 == 0 && s % 3 == 0))
                {
                    g.setColour (theme.text.withAlpha (0.35f));
                    g.drawText (juce::String (s + 1), cell.translated (0, cell.getHeight() + 2).withHeight (12).toNearestInt(), juce::Justification::centred);
                }
            }
        }
        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("RASTER", r.reduced (12.0f, 6.0f), juce::Justification::topLeft);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String::fromUTF8 ("Klicken zum Umschalten \xc2\xb7 Dreier-Gruppen = Dreivierteltakt"), r.reduced (12.0f, 6.0f), juce::Justification::topRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int row = 0; row < 2; ++row)
            for (int s = 0; s < pascher::numSteps; ++s)
                if (cellBounds (row, s).expanded (2.0f).contains (e.position))
                {
                    auto* p = proc.apvts.getParameter (row == 0 ? pascher::gateId (s) : pascher::clapId (s));
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (value (row, s) ? 0.0f : 1.0f);
                    p->endChangeGesture();
                    repaint();
                    return;
                }
    }

private:
    static constexpr float labelW = 96.0f;
    juce::Rectangle<float> rowBounds (int row) const
    {
        auto r = getLocalBounds().toFloat().reduced (14.0f, 10.0f).withTrimmedTop (22.0f);
        const float h = r.getHeight() / 2.0f;
        return r.withTop (r.getY() + row * h).withHeight (h - 14.0f);
    }
    juce::Rectangle<float> cellBounds (int row, int s) const
    {
        auto ra = rowBounds (row).withTrimmedLeft (labelW);
        const float w = ra.getWidth() / pascher::numSteps;
        const float gap = (s % 4 == 0 && s > 0) ? 4.0f : 2.0f;
        return juce::Rectangle<float> (ra.getX() + s * w + gap, ra.getY(), w - gap - 2.0f, ra.getHeight());
    }
    bool value (int row, int s) const
    {
        return proc.apvts.getRawParameterValue (row == 0 ? pascher::gateId (s) : pascher::clapId (s))->load() > 0.5f;
    }
    void timerCallback() override { repaint(); }

    PascherProcessor& proc;
    tmk::Theme theme;
};

class PascherEditor : public tmk::Editor
{
public:
    explicit PascherEditor (PascherProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig();
    StepGrid grid;
};

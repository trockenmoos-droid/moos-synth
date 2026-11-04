#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

// Rotierendes Kochsalz-Gitter (Na+/Cl-, kubisch). Jedes Korn ist ein leuchtendes Teilchen:
// x = Panorama, y = Tonhöhe, z = Tiefe im Salzlager. Mit „Kristall“ rasten die Teilchen auf Gitterplätze ein,
// bei wenig Kristall löst sich das Gitter auf und die Körner treiben frei wie in Sole.
class SaltLattice : public juce::Component, private juce::Timer
{
public:
    SaltLattice (SalzProcessor& p, const tmk::Theme& t) : proc (p), theme (t) { startTimerHz (40); }
    ~SaltLattice() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x66000000));
        g.fillRoundedRectangle (r, 6.0f);
        auto area = r.reduced (12.0f).withTrimmedTop (14.0f);
        const float scale = juce::jmin (area.getWidth(), area.getHeight()) * 0.27f;
        const auto c = area.getCentre();
        const float k = proc.crystalNow.load();

        auto project = [&] (float x, float y, float z, float& depth) -> juce::Point<float>
        {
            const float ca = std::cos (yaw), sa = std::sin (yaw), cb = std::cos (pitch), sb = std::sin (pitch);
            const float x1 = x * ca + z * sa, z1 = -x * sa + z * ca;
            const float y1 = y * cb - z1 * sb, z2 = y * sb + z1 * cb;
            depth = z2;
            const float persp = 5.0f / (5.0f + z2);
            return { c.x + x1 * scale * persp, c.y - y1 * scale * persp };
        };

        // Gitter: 5 x 5 x 5 Plätze
        constexpr int N = 5;
        auto node = [] (int i) { return -1.0f + 2.0f * (float) i / (N - 1); };
        const float bondA = 0.06f + 0.22f * k;
        g.setColour (theme.text.withAlpha (bondA));
        for (int a = 0; a < N; ++a)
            for (int b = 0; b < N; ++b)
                for (int axis = 0; axis < 3; ++axis)
                {
                    float d0, d1;
                    juce::Point<float> p0, p1;
                    if (axis == 0) { p0 = project (-1, node (a), node (b), d0); p1 = project (1, node (a), node (b), d1); }
                    else if (axis == 1) { p0 = project (node (a), -1, node (b), d0); p1 = project (node (a), 1, node (b), d1); }
                    else { p0 = project (node (a), node (b), -1, d0); p1 = project (node (a), node (b), 1, d1); }
                    g.drawLine ({ p0, p1 }, 0.7f);
                }
        // Ionen
        struct Dot { juce::Point<float> p; float d; bool cl; };
        std::vector<Dot> dots;
        for (int a = 0; a < N; ++a) for (int b = 0; b < N; ++b) for (int e = 0; e < N; ++e)
        {
            float d; auto p = project (node (a), node (b), node (e), d);
            dots.push_back ({ p, d, ((a + b + e) & 1) == 0 });
        }
        std::sort (dots.begin(), dots.end(), [] (const Dot& x, const Dot& y) { return x.d > y.d; });
        for (auto& dt : dots)
        {
            const float sz = (dt.cl ? 7.0f : 4.5f) * (5.0f / (5.0f + dt.d)) * (0.6f + 0.4f * k);
            const float alpha = (0.25f + 0.45f * k) * juce::jlimit (0.3f, 1.0f, 1.0f - dt.d * 0.3f);
            g.setColour ((dt.cl ? juce::Colour (0xffd8f0e4) : juce::Colour (0xffb59ae0)).withAlpha (alpha));
            g.fillEllipse (juce::Rectangle<float> (sz, sz).withCentre (dt.p));
        }

        // Körner
        for (int i = 0; i < salz::maxGrains; ++i)
        {
            const float glow = proc.view[i].glow.load();
            if (glow < 0.02f) continue;
            float x = juce::jlimit (-1.0f, 1.0f, proc.view[i].x.load());
            float y = juce::jlimit (-1.0f, 1.0f, proc.view[i].y.load());
            float z = juce::jlimit (-1.0f, 1.0f, proc.view[i].z.load() * 2.0f - 1.0f);
            // Sole: Teilchen zittern frei; Kristall: auf den nächsten Gitterplatz
            const float jit = (1.0f - k) * 0.12f;
            x += jitter (i, 0) * jit; y += jitter (i, 1) * jit; z += jitter (i, 2) * jit;
            auto snapTo = [] (float v) { return std::round ((v + 1.0f) * 2.0f) / 2.0f - 1.0f; };
            x += (snapTo (x) - x) * k; y += (snapTo (y) - y) * k; z += (snapTo (z) - z) * k;
            float d; auto p = project (x, y, z, d);
            const float sz = (6.0f + 16.0f * glow) * (5.0f / (5.0f + d));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff).withAlpha (glow), p.x, p.y,
                                                     theme.accent.withAlpha (0.0f), p.x + sz, p.y, true));
            g.fillEllipse (juce::Rectangle<float> (sz * 2.0f, sz * 2.0f).withCentre (p));
        }

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (theme.dim);
        g.drawText (juce::String::fromUTF8 (k < 0.33f ? "Sole \xe2\x80\x93 die K\xc3\xb6rner treiben frei"
                                          : k < 0.66f ? "Salz f\xc3\xa4llt aus"
                                                      : "Kristall \xe2\x80\x93 alles rastet ins Gitter"),
                    r.reduced (12.0f, 8.0f), juce::Justification::bottomLeft);
        const int ng = proc.activeGrains.load();
        g.drawText (juce::String (ng) + juce::String::fromUTF8 (ng == 1 ? " Korn" : " K\xc3\xb6rner"), r.reduced (12.0f, 8.0f), juce::Justification::bottomRight);
        g.drawText (juce::String::fromUTF8 ("x Raum \xc2\xb7 y Tonh\xc3\xb6he \xc2\xb7 z Tiefe \xe2\x80\x93 ziehen zum Drehen"), r.reduced (12.0f, 8.0f), juce::Justification::topRight);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        yaw = dragYaw + e.getDistanceFromDragStartX() * 0.01f;
        pitch = juce::jlimit (-1.3f, 1.3f, dragPitch + e.getDistanceFromDragStartY() * 0.01f);
    }
    void mouseDown (const juce::MouseEvent&) override { dragYaw = yaw; dragPitch = pitch; dragging = true; }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    float jitter (int i, int axis) const
    {
        const float t = phase * (0.7f + 0.13f * (float) ((i * 7 + axis * 3) % 5));
        return std::sin (t + (float) i * 1.7f + (float) axis * 2.1f);
    }
    void timerCallback() override
    {
        phase += 0.05f;
        if (! dragging) yaw += 0.004f + 0.01f * (1.0f - proc.crystalNow.load());
        repaint();
    }
    SalzProcessor& proc;
    tmk::Theme theme;
    float yaw = 0.6f, pitch = 0.45f, phase = 0.0f, dragYaw = 0.0f, dragPitch = 0.0f;
    bool dragging = false;
};

class SalzEditor : public tmk::Editor
{
public:
    explicit SalzEditor (SalzProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig();
    SaltLattice lattice;
};

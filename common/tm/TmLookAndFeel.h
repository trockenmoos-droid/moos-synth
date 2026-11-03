#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace tmk
{
    // Farbschema eines Plugins (jedes Plugin der Serie hat seinen eigenen Akzent)
    struct Theme
    {
        juce::Colour accent  { 0xffa8d65c };
        juce::Colour text    { 0xffe4eec8 };
        juce::Colour dim     { 0xff9aa98a };
        juce::Colour glass   { 0x880b1009 };
        juce::Colour outline { 0x40c6e07a };
        juce::Colour bg      { 0xff0d120b };
        juce::Colour knobTop { 0xff3a4630 };
        juce::Colour knobBot { 0xff141a10 };
    };

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel (const Theme& t) : theme (t)
        {
            setColour (juce::Slider::textBoxTextColourId, theme.text);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x50000000));
            setColour (juce::Label::textColourId, theme.text);
            setColour (juce::ComboBox::textColourId, theme.text);
            setColour (juce::ComboBox::arrowColourId, theme.accent);
            setColour (juce::PopupMenu::backgroundColourId, theme.bg.withAlpha (0.96f));
            setColour (juce::PopupMenu::textColourId, theme.text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent.withAlpha (0.85f));
            setColour (juce::PopupMenu::highlightedTextColourId, theme.bg);
            setColour (juce::PopupMenu::headerTextColourId, theme.accent);
            setColour (juce::TextButton::textColourOffId, theme.text);
            setColour (juce::TextButton::textColourOnId, theme.bg);
            setColour (juce::TextEditor::backgroundColourId, theme.bg.brighter (0.1f));
            setColour (juce::TextEditor::textColourId, theme.text);
        }

        const Theme theme;

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& s) override
        {
            auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
            auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
            auto centre = bounds.getCentre();
            auto angle  = startAngle + pos * (endAngle - startAngle);
            const float lw = 3.5f;

            juce::Path track;
            track.addCentredArc (centre.x, centre.y, radius - lw, radius - lw, 0.0f, startAngle, endAngle, true);
            g.setColour (juce::Colour (0x50000000));
            g.strokePath (track, juce::PathStrokeType (lw + 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
            const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, radius - lw, radius - lw, 0.0f,
                                 juce::jmin (from, angle), juce::jmax (from, angle), true);
            g.setColour (theme.accent);
            g.strokePath (value, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            auto knob = juce::Rectangle<float> (radius * 1.25f, radius * 1.25f).withCentre (centre);
            g.setGradientFill (juce::ColourGradient (theme.knobTop, knob.getX(), knob.getY(),
                                                     theme.knobBot, knob.getRight(), knob.getBottom(), false));
            g.fillEllipse (knob);
            g.setColour (juce::Colour (0x30ffffff));
            g.drawEllipse (knob, 1.0f);

            juce::Point<float> tip (centre.x + radius * 0.55f * std::sin (angle), centre.y - radius * 0.55f * std::cos (angle));
            g.setColour (theme.text);
            g.drawLine ({ centre.getPointOnCircumference (radius * 0.12f, angle), tip }, 2.0f);
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                               juce::Slider::SliderStyle, juce::Slider& s) override
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
            g.setColour (juce::Colour (0x60000000));
            g.fillRoundedRectangle (r, 3.0f);
            const float mid = s.getMinimum() < 0.0 ? r.getCentreX() : r.getX();
            g.setColour (theme.accent);
            g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (mid, pos), r.getY(),
                                                                                juce::jmax (mid, pos), r.getBottom()), 3.0f);
            g.setColour (theme.text);
            g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pos, r.getCentreY() }));
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const bool on = b.getToggleState();
            g.setColour (on ? theme.accent : juce::Colour (over ? 0x70000000 : 0x50000000));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (on ? theme.accent.brighter (0.3f) : theme.outline.withMultipliedAlpha (down ? 2.0f : 1.4f));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
            g.setColour (juce::Colour (0x60000000));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (theme.outline.withMultipliedAlpha (1.4f));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
            juce::Path arrow;
            const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
            arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
            g.setColour (theme.accent);
            g.fillPath (arrow);
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override          { return juce::FontOptions (13.0f); }
        juce::Font getTextButtonFont (juce::TextButton&, int) override  { return juce::FontOptions (12.5f, juce::Font::bold); }
        juce::Font getPopupMenuFont() override                         { return juce::FontOptions (14.0f); }
    };
}

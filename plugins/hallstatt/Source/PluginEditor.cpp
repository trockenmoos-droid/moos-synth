#include "PluginEditor.h"

static tmk::Theme hallstattTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xffe8b46a);   // warmes Abendlicht
    t.text    = juce::Colour (0xffeef0f4);
    t.dim     = juce::Colour (0xff9aa6b8);
    t.glass   = juce::Colour (0x8a0a1220);
    t.outline = juce::Colour (0x40e8c79a);
    t.bg      = juce::Colour (0xff0a111c);
    t.knobTop = juce::Colour (0xff3a4558);
    t.knobBot = juce::Colour (0xff121822);
    return t;
}

tmk::Editor::Config HallstattEditor::makeConfig()
{
    Config c;
    c.title = "HALLSTATT";
    c.subtitle = "reverb";
    c.textureName = "hallstatt";
    c.texture = [] { return tmk::tex::lakeAtDusk(); };
    c.theme = hallstattTheme();
    c.width = 1040; c.height = 660;
    return c;
}

HallstattEditor::HallstattEditor (HallstattProcessor& p)
    : tmk::Editor (p, makeConfig()), view (p, cfg.theme)
{
    addSection ("RAUM", { combo ("room", "Raum", 2.0f), toggle ("freeze", "FREEZE", 1.0f) });
    addSection ("HALLKURVE", { custom (view, 1.0f) });
    addSection ("RAUMFORM", { knob ("size", u8 ("Gr\xc3\xb6\xc3\x9f" "e")), knob ("decay", "Nachhall"),
                              knob ("predelay", "Predelay"), knob ("early", u8 ("Fr\xc3\xbch" "e Refl.")) });
    addSection ("KLANG", { knob ("damping", u8 ("D\xc3\xa4mpfung")), knob ("bass", "Bass"),
                           knob ("mod", "Modulation"), knob ("width", "Breite") });
    addSection ("FILTER", { knob ("lowcut", "Low Cut"), knob ("highcut", "High Cut") });
    addSection ("MIX", { knob ("duck", "Ducking"), knob ("mix", "Mix") });
    finishSetup();
}

void HallstattEditor::layoutBody (juce::Rectangle<int> body)
{
    const int rowH = body.getHeight() / 3;
    auto row1 = body.removeFromTop (rowH + 30);
    auto r1 = split (row1, { 3, 7 });
    layoutSection ("RAUM", r1[0]);
    layoutSection ("HALLKURVE", r1[1]);

    auto row2 = body.removeFromTop ((body.getHeight()) / 2);
    auto r2 = split (row2, { 1, 1 });
    layoutSection ("RAUMFORM", r2[0]);
    layoutSection ("KLANG", r2[1]);

    auto r3 = split (body, { 1, 1 });
    layoutSection ("FILTER", r3[0]);
    layoutSection ("MIX", r3[1]);
}

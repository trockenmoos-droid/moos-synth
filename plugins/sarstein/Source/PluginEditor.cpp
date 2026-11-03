#include "PluginEditor.h"

static tmk::Theme sarsteinTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xffe07a3c);   // Flechten-Orange
    t.text    = juce::Colour (0xfff3f0ea);
    t.dim     = juce::Colour (0xffb8b0a4);
    t.glass   = juce::Colour (0xa0141312);
    t.outline = juce::Colour (0x50e0c8b0);
    t.bg      = juce::Colour (0xff181614);
    t.knobTop = juce::Colour (0xff5a5650);
    t.knobBot = juce::Colour (0xff1c1a18);
    return t;
}

tmk::Editor::Config SarsteinEditor::makeConfig()
{
    Config c;
    c.title = "SARSTEIN";
    c.subtitle = "distortion";
    c.textureName = "sarstein";
    c.texture = [] { return tmk::tex::limestone(); };
    c.theme = sarsteinTheme();
    c.width = 1040; c.height = 660;
    return c;
}

SarsteinEditor::SarsteinEditor (SarsteinProcessor& p)
    : tmk::Editor (p, makeConfig()), view (p, cfg.theme)
{
    addSection ("GESTEIN", { combo ("stein", "Gestein", 1.6f), toggle ("autogain", "AUTO-PEGEL", 1.2f) });
    addSection ("KENNLINIE", { custom (view, 1.0f) });
    addSection ("VERZERRUNG", { knob ("drive", "Drive"), knob ("geroell", u8 ("Ger\xc3\xb6ll")), knob ("steinschlag", "Steinschlag") });
    addSection ("KLANG", { knob ("tiefe", "Tiefe"), knob ("ton", "Ton"), knob ("highcut", "High Cut") });
    addSection ("AUSGANG", { knob ("mix", "Mix"), knob ("output", "Ausgang") });
    finishSetup();
}

void SarsteinEditor::layoutBody (juce::Rectangle<int> body)
{
    const int rowH = body.getHeight() / 3;
    auto r1 = split (body.removeFromTop (rowH + 30), { 3, 7 });
    layoutSection ("GESTEIN", r1[0]);
    layoutSection ("KENNLINIE", r1[1]);
    auto r2 = split (body.removeFromTop (body.getHeight() / 2), { 1, 1 });
    layoutSection ("VERZERRUNG", r2[0]);
    layoutSection ("KLANG", r2[1]);
    // unten rechts bleibt frei: ein Fenster auf die Felswand
    layoutSection ("AUSGANG", split (body, { 4, 6 })[0]);
}

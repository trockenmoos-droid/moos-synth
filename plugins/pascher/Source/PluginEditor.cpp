#include "PluginEditor.h"

static tmk::Theme pascherTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xff8fd06a);   // Stickerei-Grün
    t.text    = juce::Colour (0xfff6efe0);
    t.dim     = juce::Colour (0xffd2bc98);
    t.glass   = juce::Colour (0x9a1a0f07);
    t.outline = juce::Colour (0x50e8d6a8);
    t.bg      = juce::Colour (0xff1e130a);
    t.knobTop = juce::Colour (0xff6e4a2c);
    t.knobBot = juce::Colour (0xff24160a);
    return t;
}

tmk::Editor::Config PascherEditor::makeConfig()
{
    Config c;
    c.title = "GOISERER PASCHER";
    c.subtitle = "clap & gate";
    c.textureName = "pascher";
    c.texture = [] { return tmk::tex::leather(); };
    c.theme = pascherTheme();
    c.width = 1100; c.height = 680;
    return c;
}

PascherEditor::PascherEditor (PascherProcessor& p)
    : tmk::Editor (p, makeConfig()), grid (p, cfg.theme)
{
    addSection ("MUSTER", { custom (grid, 1.0f) });
    addSection ("TAKT", { combo ("rate", "Raster", 1.1f), knob ("laenge", u8 ("L\xc3\xa4nge")), knob ("swing", "Swing"), knob ("schmuggel", "Schmuggel") });
    addSection ("GATE", { knob ("tiefe", "Tiefe"), knob ("glaetten", u8 ("Gl\xc3\xa4tten")), knob ("mix", "Mix") });
    addSection ("KLATSCHER", { knob ("klatsch", "Pegel"), knob ("klatscher", "Anzahl"), knob ("mensch", "Mensch"),
                               knob ("ton", u8 ("H\xc3\xa4nde")), knob ("raum", "Wirtshaus") });
    addSection ("AUSGANG", { knob ("output", "Ausgang") });
    finishSetup();
}

void PascherEditor::layoutBody (juce::Rectangle<int> body)
{
    const int h = body.getHeight();
    layoutSection ("MUSTER", body.removeFromTop (h * 36 / 100));
    auto r2 = split (body.removeFromTop (body.getHeight() / 2), { 4, 3 });
    layoutSection ("TAKT", r2[0]);
    layoutSection ("GATE", r2[1]);
    auto r3 = split (body, { 5, 1 });
    layoutSection ("KLATSCHER", r3[0]);
    layoutSection ("AUSGANG", r3[1]);
}

#include "PluginEditor.h"

static tmk::Theme soleTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xfff2ead8);   // Salzweiß
    t.text    = juce::Colour (0xfff7efe2);
    t.dim     = juce::Colour (0xffc0a888);
    t.glass   = juce::Colour (0x9a140c06);
    t.outline = juce::Colour (0x50f2dcb8);
    t.bg      = juce::Colour (0xff1a0f07);
    t.knobTop = juce::Colour (0xff6a4a2e);
    t.knobBot = juce::Colour (0xff241508);
    return t;
}

tmk::Editor::Config SoleleitungEditor::makeConfig()
{
    Config c;
    c.title = "SOLELEITUNG";
    c.subtitle = "delay";
    c.textureName = "soleleitung";
    c.texture = [] { return tmk::tex::woodPipe(); };
    c.theme = soleTheme();
    c.width = 1040; c.height = 660;
    return c;
}

SoleleitungEditor::SoleleitungEditor (SoleleitungProcessor& p)
    : tmk::Editor (p, makeConfig()), view (p, cfg.theme)
{
    addSection ("LEITUNG", { custom (view, 1.0f) });
    addSection ("ZEIT", { toggle ("sync", "SYNC"), combo ("division", "Takt", 1.2f), knob ("time", "Zeit ms"),
                          knob ("feedback", "Feedback"), knob ("stations", "Stationen") });
    addSection ("ROHR", { knob ("rohr", "Rohr"), knob ("rohrTon", "Rohrton") });
    addSection (u8 ("SALZ & STR\xc3\x96MUNG"), { knob ("salz", "Salz"), knob ("stroemung", u8 ("Str\xc3\xb6mung")) });
    addSection ("FILTER", { knob ("lowcut", "Low Cut"), knob ("ton", "Ton") });
    addSection ("AUSGANG", { knob ("breite", "Breite"), knob ("duck", "Ducking"), knob ("mix", "Mix"), toggle ("absperren", "ABSPERREN", 1.2f) });
    finishSetup();
}

void SoleleitungEditor::layoutBody (juce::Rectangle<int> body)
{
    layoutSection ("LEITUNG", body.removeFromTop (body.getHeight() * 36 / 100));
    auto row2 = body.removeFromTop (body.getHeight() / 2);
    auto r2 = split (row2, { 5, 2 });
    layoutSection ("ZEIT", r2[0]);
    layoutSection ("ROHR", r2[1]);
    auto r3 = split (body, { 2, 2, 4 });
    layoutSection (u8 ("SALZ & STR\xc3\x96MUNG"), r3[0]);
    layoutSection ("FILTER", r3[1]);
    layoutSection ("AUSGANG", r3[2]);
}

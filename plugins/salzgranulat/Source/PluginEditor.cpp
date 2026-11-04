#include "PluginEditor.h"

static tmk::Theme saltTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xfff2b8c6);   // rosa Steinsalz
    t.text    = juce::Colour (0xfff4f6f8);
    t.dim     = juce::Colour (0xffb4bcc8);
    t.glass   = juce::Colour (0x9c10141c);
    t.outline = juce::Colour (0x50e8eef8);
    t.bg      = juce::Colour (0xff0d1018);
    t.knobTop = juce::Colour (0xff6a6e7a);
    t.knobBot = juce::Colour (0xff1c1f28);
    return t;
}

tmk::Editor::Config SalzEditor::makeConfig()
{
    Config c;
    c.title = "SALZGRANULAT";
    c.subtitle = "kristall-granulator";
    c.textureName = "salzgranulat";
    c.texture = [] { return tmk::tex::saltCrystals(); };
    c.theme = saltTheme();
    c.width = 1240; c.height = 760;
    return c;
}

SalzEditor::SalzEditor (SalzProcessor& p)
    : tmk::Editor (p, makeConfig()), lattice (p, cfg.theme)
{
    addSection ("SALZGITTER", { custom (lattice, 1.0f) });
    addSection (u8 ("K\xc3\x96RNER"), { knob ("korn", "Korn"), knob ("dichte", "Dichte"), knob ("tiefe", "Tiefe"),
                                        knob ("streuung", "Streuung"), knob ("rueckwaerts", u8 ("R\xc3\xbc" "ckw\xc3\xa4rts")) });
    addSection ("KRISTALL", { knob ("kristall", "Kristall", 1.2f), combo ("gitter", "Gitter", 1.4f), combo ("grundton", "Grundton", 1.0f),
                              knob ("lage", "Lage"), knob ("klang", "Klang") });
    addSection ("VERWANDLUNG", { knob ("verdunstung", "Verdunstung"), knob ("rekristall", "Rekristall."), knob ("anschlag", "Anschlag"),
                                 toggle ("kruste", "Salzkruste", 1.1f) });
    addSection ("AUSGANG", { knob ("breite", "Breite"), knob ("mix", "Mix"), knob ("ausgang", "Ausgang") });
    finishSetup();
}

void SalzEditor::layoutBody (juce::Rectangle<int> body)
{
    layoutSection ("SALZGITTER", body.removeFromLeft (body.getWidth() * 44 / 100));
    const int h = body.getHeight() / 3;
    layoutSection (juce::String::fromUTF8 ("K\xc3\x96RNER"), body.removeFromTop (h));
    layoutSection ("KRISTALL", body.removeFromTop (h));
    auto last = split (body, { 4, 3 });
    layoutSection ("VERWANDLUNG", last[0]);
    layoutSection ("AUSGANG", last[1]);
}

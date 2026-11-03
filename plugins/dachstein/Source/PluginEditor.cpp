#include "PluginEditor.h"

static tmk::Theme dachsteinTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xff7fd8f0);   // Gletscherblau
    t.text    = juce::Colour (0xfff2fbff);
    t.dim     = juce::Colour (0xffa8c6d6);
    t.glass   = juce::Colour (0x9a04182a);
    t.outline = juce::Colour (0x5ab8ecff);
    t.bg      = juce::Colour (0xff06182a);
    t.knobTop = juce::Colour (0xff3e6a86);
    t.knobBot = juce::Colour (0xff0b2234);
    return t;
}

tmk::Editor::Config DachsteinEditor::makeConfig (DachsteinProcessor& p)
{
    Config c;
    c.title = "DACHSTEIN";
    c.subtitle = "glacier drone";
    c.textureName = "dachstein";
    c.texture = [] { return tmk::tex::glacierIce(); };
    c.theme = dachsteinTheme();
    c.width = 1100; c.height = 740;
    c.keyboardState = &p.keyboardState;
    return c;
}

DachsteinEditor::DachsteinEditor (DachsteinProcessor& p)
    : tmk::Editor (p, makeConfig (p)), view (p, cfg.theme)
{
    addSection ("SPEKTRUM", { custom (view, 1.0f) });
    addSection (u8 ("OBERT\xc3\x96NE"), { knob ("obertoene", "Helligkeit"), knob ("kristall", "Kristall"),
                                           knob ("spalten", "Spalten"), knob ("teiltoene", u8 ("Teilt\xc3\xb6ne")) });
    addSection ("BEWEGUNG", { knob ("bewegung", "Bewegung"), knob ("tempo", "Tempo"), knob ("schmelze", "Schmelze"), knob ("breite", "Breite") });
    addSection ("FELS & WIND", { knob ("fels", "Fels"), knob ("wind", "Wind"), knob ("windFarbe", "Farbe"), knob ("boeen", u8 ("B\xc3\xb6" "en")) });
    addSection ("EIS", { knob ("knistern", "Knistern") });
    addSection (u8 ("H\xc3\x9cLLE"), { knob ("attack", "Attack"), knob ("release", "Release") });
    addSection ("AUSGANG", { knob ("hall", "Hall"), knob ("volume", u8 ("Lautst\xc3\xa4rke")) });
    finishSetup();
}

void DachsteinEditor::layoutBody (juce::Rectangle<int> body)
{
    const int rowH = body.getHeight() / 3;
    auto r1 = split (body.removeFromTop (rowH), { 11, 9 });
    layoutSection ("SPEKTRUM", r1[0]);
    layoutSection (u8 ("OBERT\xc3\x96NE"), r1[1]);
    auto r2 = split (body.removeFromTop (rowH), { 1, 1 });
    layoutSection ("BEWEGUNG", r2[0]);
    layoutSection ("FELS & WIND", r2[1]);
    auto r3 = split (body, { 2, 4, 4 });
    layoutSection ("EIS", r3[0]);
    layoutSection (u8 ("H\xc3\x9cLLE"), r3[1]);
    layoutSection ("AUSGANG", r3[2]);
}

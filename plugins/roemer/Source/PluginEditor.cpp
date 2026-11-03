#include "PluginEditor.h"

static tmk::Theme roemerTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xffe0743c);   // Terrakotta
    t.text    = juce::Colour (0xfff6efe2);
    t.dim     = juce::Colour (0xffcdbfa4);
    t.glass   = juce::Colour (0xa018140f);
    t.outline = juce::Colour (0x50e6c89a);
    t.bg      = juce::Colour (0xff1c1712);
    t.knobTop = juce::Colour (0xff6a584a);
    t.knobBot = juce::Colour (0xff221a14);
    return t;
}

tmk::Editor::Config RoemerEditor::makeConfig (RoemerProcessor& p)
{
    Config c;
    c.title = juce::String::fromUTF8 ("R\xc3\x96MER");
    c.subtitle = "noricum";
    c.textureName = "roemer";
    c.texture = [] { return tmk::tex::romanMosaic(); };
    c.theme = roemerTheme();
    c.width = 1100; c.height = 740;
    c.keyboardState = &p.keyboardState;
    return c;
}

RoemerEditor::RoemerEditor (RoemerProcessor& p)
    : tmk::Editor (p, makeConfig (p)),
      view (p.scope, cfg.theme, "HYDRAULIS & CORNU", [] { return juce::String::fromUTF8 ("Noricum \xc2\xb7 15 v. Chr. \xe2\x80\x93 476 n. Chr."); })
{
    addSection ("ANSICHT", { custom (view, 1.0f) });
    addSection ("HYDRAULIS", { knob ("hydraulis", "Pegel"), knob ("wasserdruck", "Wasserdruck"), knob ("blubbern", "Blubbern"),
                               knob ("luft", "Luft"), knob ("reihen", "Reihen") });
    addSection ("CORNU", { knob ("cornu", "Pegel"), knob ("hell", "Helligkeit"), knob ("anblasen", "Anblasen") });
    addSection (u8 ("H\xc3\x9cLLE"), { knob ("attack", "Attack"), knob ("release", "Release") });
    addSection ("ORT", { combo ("raum", "Ort", 1.2f), knob ("hall", "Hall"), knob ("volume", u8 ("Lautst\xc3\xa4rke")) });
    finishSetup();
}

void RoemerEditor::layoutBody (juce::Rectangle<int> body)
{
    const int rowH = body.getHeight() / 3;
    layoutSection ("ANSICHT", split (body.removeFromTop (rowH), { 1, 1, 1 })[1]);   // mittig, Mosaik links und rechts sichtbar
    auto r2 = split (body.removeFromTop (rowH), { 5, 3 });
    layoutSection ("HYDRAULIS", r2[0]);
    layoutSection ("CORNU", r2[1]);
    auto r3 = split (body, { 2, 3 });
    layoutSection (u8 ("H\xc3\x9cLLE"), r3[0]);
    layoutSection ("ORT", r3[1]);
}

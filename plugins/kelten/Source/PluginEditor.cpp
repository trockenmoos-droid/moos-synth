#include "PluginEditor.h"

static tmk::Theme keltenTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xff6fd0b0);   // Patina-Grün
    t.text    = juce::Colour (0xfff5ecd8);
    t.dim     = juce::Colour (0xffc8b48c);
    t.glass   = juce::Colour (0x9a140e08);
    t.outline = juce::Colour (0x50e0b870);
    t.bg      = juce::Colour (0xff1a1208);
    t.knobTop = juce::Colour (0xff6a4a28);
    t.knobBot = juce::Colour (0xff20140a);
    return t;
}

tmk::Editor::Config KeltenEditor::makeConfig (KeltenProcessor& p)
{
    Config c;
    c.title = "KELTEN";
    c.subtitle = "hallstatt culture";
    c.textureName = "kelten";
    c.texture = [] { return tmk::tex::celticBronze(); };
    c.theme = keltenTheme();
    c.width = 1100; c.height = 740;
    c.keyboardState = &p.keyboardState;
    return c;
}

KeltenEditor::KeltenEditor (KeltenProcessor& p)
    : tmk::Editor (p, makeConfig (p)),
      view (p.scope, cfg.theme, "CARNYX", [] { return juce::String::fromUTF8 ("Eisenzeit \xc2\xb7 800 \xe2\x80\x93 450 v. Chr."); })
{
    addSection ("ANSICHT", { custom (view, 1.0f) });
    addSection ("CARNYX", { knob ("carnyx", "Pegel"), knob ("growl", "Growl"), knob ("zunge", "Zunge"), knob ("anblasen", "Anblasen"),
                            knob ("hell", "Helligkeit"), knob ("attack", "Attack"), knob ("release", "Release") });
    addSection ("BORDUN", { knob ("bordun", "Pegel"), combo ("bordunTon", "Ton", 1.1f), knob ("quinte", "Quinte") });
    addSection ("EISENZEIT", { knob ("erde", "Erde"), knob ("rost", "Rost"), knob ("alter", "Alter") });
    addSection ("AUSGANG", { knob ("hall", u8 ("H\xc3\xb6hle")), knob ("volume", u8 ("Lautst\xc3\xa4rke")) });
    finishSetup();
}

void KeltenEditor::layoutBody (juce::Rectangle<int> body)
{
    const int rowH = body.getHeight() / 3;
    layoutSection ("ANSICHT", split (body.removeFromTop (rowH), { 5, 4 })[0]);
    layoutSection ("CARNYX", body.removeFromTop (rowH));
    auto r3 = split (body, { 3, 3, 2 });
    layoutSection ("BORDUN", r3[0]);
    layoutSection ("EISENZEIT", r3[1]);
    layoutSection ("AUSGANG", r3[2]);
}

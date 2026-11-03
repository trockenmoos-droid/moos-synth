#include "PluginEditor.h"

static tmk::Theme wolfgangTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xffe8c25a);   // Altargold
    t.text    = juce::Colour (0xfff8f1e0);
    t.dim     = juce::Colour (0xffc8b894);
    t.glass   = juce::Colour (0x90120e0a);
    t.outline = juce::Colour (0x50e8c25a);
    t.bg      = juce::Colour (0xff140f0a);
    t.knobTop = juce::Colour (0xff5a4a30);
    t.knobBot = juce::Colour (0xff1a140c);
    return t;
}

tmk::Editor::Config WolfgangEditor::makeConfig (WolfgangProcessor& p)
{
    Config c;
    c.title = "SANKT WOLFGANG";
    c.subtitle = "bells & organ";
    c.textureName = "sanktwolfgang";
    c.texture = [] { return tmk::tex::stainedGlass(); };
    c.theme = wolfgangTheme();
    c.width = 1160; c.height = 780;
    c.keyboardState = &p.keyboardState;
    return c;
}

WolfgangEditor::WolfgangEditor (WolfgangProcessor& p)
    : tmk::Editor (p, makeConfig (p)), view (p, cfg.theme)
{
    addSection ("ANSICHT", { custom (view, 1.0f) });
    addSection ("GLOCKE", { knob ("glocke", "Pegel"), knob ("groesse", u8 ("Gr\xc3\xb6\xc3\x9f" "e")), knob ("terz", "Terz"),
                            knob ("material", "Material"), knob ("schwebung", "Schwebung"), knob ("anschlag", "Anschlag"),
                            knob ("daempfung", u8 ("D\xc3\xa4mpfen")) });
    addSection ("REGISTER", { knob ("r16", "16'"), knob ("r8", "8'"), knob ("r4", "4'"), knob ("r223", "2 2/3'"),
                              knob ("r2", "2'"), knob ("r135", "1 3/5'"), knob ("mixtur", "Mixtur") });
    addSection ("ORGEL", { knob ("orgel", "Pegel"), knob ("chiff", "Chiff"), knob ("wind", "Wind"), knob ("tremulant", "Tremulant") });
    addSection (u8 ("ORGEL-H\xc3\x9cLLE"), { knob ("oAttack", "Attack"), knob ("oRelease", "Release") });
    addSection ("KIRCHE", { knob ("hall", "Hall"), knob ("hallZeit", "Hallzeit") });
    addSection ("AUSGANG", { knob ("volume", u8 ("Lautst\xc3\xa4rke")) });
    finishSetup();
}

void WolfgangEditor::layoutBody (juce::Rectangle<int> body)
{
    const int rowH = body.getHeight() / 3;
    auto r1 = split (body.removeFromTop (rowH), { 9, 14 });
    layoutSection ("ANSICHT", r1[0]);
    layoutSection ("GLOCKE", r1[1]);
    auto r2 = split (body.removeFromTop (rowH), { 14, 9 });
    layoutSection ("REGISTER", r2[0]);
    layoutSection ("ORGEL", r2[1]);
    auto r3 = split (body, { 2, 2, 1 });
    layoutSection (u8 ("ORGEL-H\xc3\x9cLLE"), r3[0]);
    layoutSection ("KIRCHE", r3[1]);
    layoutSection ("AUSGANG", r3[2]);
}

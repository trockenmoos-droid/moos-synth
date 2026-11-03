#include "PluginEditor.h"

static tmk::Theme forgeTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xffff8a2a);   // Glut
    t.text    = juce::Colour (0xfff2eee8);
    t.dim     = juce::Colour (0xffb8b2aa);
    t.glass   = juce::Colour (0xa00c0d10);
    t.outline = juce::Colour (0x50c8a080);
    t.bg      = juce::Colour (0xff0e0f11);
    t.knobTop = juce::Colour (0xff5a5d62);
    t.knobBot = juce::Colour (0xff1a1b1e);
    return t;
}

tmk::Editor::Config HammerEditor::makeConfig()
{
    Config c;
    c.title = "HAMMERSCHMIED";
    c.subtitle = "generative granular drums";
    c.textureName = "hammerschmied";
    c.texture = [] { return tmk::tex::forgedIron(); };
    c.theme = forgeTheme();
    c.width = 1240; c.height = 820;
    return c;
}

HammerEditor::HammerEditor (HammerProcessor& p)
    : tmk::Editor (p, makeConfig()), rings (p, cfg.theme)
{
    addSection ("SCHMIEDE", { custom (rings, 1.0f) });
    addSection ("WERK", { toggle ("frei", "Frei"), knob ("swing", "Swing"), knob ("chaos", "Chaos"), knob ("mutation", "Mutation"),
                          knob ("hitze", "Hitze"), knob ("volume", u8 ("Lautst\xc3\xa4rke")) });
    addSection ("GLUT", { knob ("glut", "Glut"), knob ("korn", "Korn"), knob ("dichte", "Dichte"), knob ("tonhoehe", u8 ("Tonh\xc3\xb6he")),
                          knob ("rueckwaerts", u8 ("R\xc3\xbc" "ckw\xc3\xa4rts")), knob ("stotter", "Stotter"), knob ("streuung", "Streuung") });
    for (int t = 0; t < hammer::numTracks; ++t)
    {
        using namespace hammer;
        addSection (u8 (trackNames[t]), { knob (pid (t, "lvl"), "Pegel"), knob (pid (t, "ton"), "Ton"), knob (pid (t, "abk"), "Abkl."),
                                          knob (pid (t, "sch"), "Schritte"), knob (pid (t, "hit"), u8 ("Schl\xc3\xa4ge")), knob (pid (t, "dreh"), "Drehung"),
                                          knob (pid (t, "chance"), "Chance"), combo (pid (t, "tempo"), "Tempo", 1.2f) }).columns = 4;
    }
    finishSetup();
}

void HammerEditor::layoutBody (juce::Rectangle<int> body)
{
    auto left = body.removeFromLeft (body.getWidth() * 30 / 100);
    layoutSection ("SCHMIEDE", left);
    auto top = body.removeFromTop (body.getHeight() * 22 / 100);
    auto t2 = split (top, { 6, 7 });
    layoutSection ("WERK", t2[0]);
    layoutSection ("GLUT", t2[1]);
    const int rowH = body.getHeight() / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto r = row == 1 ? body : body.removeFromTop (rowH);
        auto cols = split (r, { 1, 1, 1 });
        for (int c = 0; c < 3; ++c)
            layoutSection (juce::String::fromUTF8 (hammer::trackNames[row * 3 + c]), cols[c]);
    }
}

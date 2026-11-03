#include "PluginEditor.h"

static tmk::Theme fingersTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xffffb489);   // Morgenrot
    t.text    = juce::Colour (0xfffff5ee);
    t.dim     = juce::Colour (0xffc8b4c4);
    t.glass   = juce::Colour (0x8a16122a);
    t.outline = juce::Colour (0x50ffc8b4);
    t.bg      = juce::Colour (0xff18142a);
    t.knobTop = juce::Colour (0xff5a4a6a);
    t.knobBot = juce::Colour (0xff1c1628);
    return t;
}

tmk::Editor::Config FingersEditor::makeConfig()
{
    Config c;
    c.title = "5 FINGERS";
    c.subtitle = "granular delay";
    c.textureName = "5fingers";
    c.texture = [] { return tmk::tex::cloudSea(); };
    c.theme = fingersTheme();
    c.width = 1100; c.height = 760;
    return c;
}

FingersEditor::FingersEditor (FingersProcessor& p)
    : tmk::Editor (p, makeConfig()), view (p, cfg.theme)
{
    addSection ("AUSSICHT", { custom (view, 1.0f) });
    for (int f = 0; f < fingers::numFingers; ++f)
    {
        auto& s = addSection ("FINGER " + juce::String (f + 1),
                              { knob (fingers::id ("zeit", f), "Zeit"), knob (fingers::id ("pegel", f), "Pegel"),
                                knob (fingers::id ("pan", f), "Pan"), knob (fingers::id ("korn", f), u8 ("K\xc3\xb6rnung")) });
        s.columns = 2;
    }
    addSection ("ZEIT", { toggle ("sync", "SYNC"), combo ("division", "Takt", 1.1f), knob ("time", u8 ("L\xc3\xa4nge")),
                          knob ("feedback", "Feedback"), knob ("ton", "Ton") });
    addSection (u8 ("K\xc3\x96RNER"), { knob ("korngroesse", u8 ("Gr\xc3\xb6\xc3\x9f" "e")), knob ("tonhoehe", u8 ("Tonh\xc3\xb6he")),
                                         knob ("streuung", "Streuung"), knob ("dichte", "Dichte"), knob ("rueckwaerts", u8 ("R\xc3\xbc" "ckw\xc3\xa4rts")) });
    addSection ("AUSGANG", { knob ("breite", "Breite"), knob ("mix", "Mix") });
    finishSetup();
}

void FingersEditor::layoutBody (juce::Rectangle<int> body)
{
    const int h = body.getHeight();
    layoutSection ("AUSSICHT", body.removeFromTop (h * 27 / 100));
    auto row2 = split (body.removeFromTop (h * 41 / 100), { 1, 1, 1, 1, 1 });
    for (int f = 0; f < fingers::numFingers; ++f) layoutSection ("FINGER " + juce::String (f + 1), row2[(size_t) f]);
    auto row3 = split (body, { 53, 52, 22 });
    layoutSection ("ZEIT", row3[0]);
    layoutSection (u8 ("K\xc3\x96RNER"), row3[1]);
    layoutSection ("AUSGANG", row3[2]);
}

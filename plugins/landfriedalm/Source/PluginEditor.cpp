#include "PluginEditor.h"

static tmk::Theme almTheme()
{
    tmk::Theme t;
    t.accent  = juce::Colour (0xff7fb2ff);   // Enzianblau, aufgehellt
    t.text    = juce::Colour (0xfff7fbf2);
    t.dim     = juce::Colour (0xffbfd1b4);
    t.glass   = juce::Colour (0x96101a0c);
    t.outline = juce::Colour (0x50d6f0c0);
    t.bg      = juce::Colour (0xff122012);
    t.knobTop = juce::Colour (0xff4a6a3a);
    t.knobBot = juce::Colour (0xff142010);
    return t;
}

tmk::Editor::Config AlmEditor::makeConfig (AlmProcessor& p)
{
    Config c;
    c.title = "LANDFRIEDALM";
    c.subtitle = "alpine instruments";
    c.textureName = "landfriedalm";
    c.texture = [] { return tmk::tex::almMeadow(); };
    c.theme = almTheme();
    c.width = 1100; c.height = 740;
    c.keyboardState = &p.keyboardState;
    return c;
}

AlmEditor::AlmEditor (AlmProcessor& p)
    : tmk::Editor (p, makeConfig (p)),
      view (p.scope, cfg.theme, "KLANG",
            [&p] { return alm::instruments[(int) p.apvts.getRawParameterValue ("instrument")->load()]; })
{
    addSection ("INSTRUMENT", { combo ("instrument", "Instrument", 1.4f), custom (view, 2.6f) });
    addSection ("KLANG", { knob ("hell", "Helligkeit"), knob ("abklingen", "Abklingen"), knob ("koerper", u8 ("K\xc3\xb6rper")),
                           knob ("schwebung", "Schwebung"), knob ("anschlag", "Anschlag") });
    addSection ("JODLER", { knob ("vokal", "Vokal"), knob ("jodelsprung", "Jodelsprung") });
    addSection ("BERG", { knob ("echo", "Echo"), knob ("echoZeit", "Echozeit"), knob ("hall", "Hall") });
    addSection ("AUSGANG", { knob ("volume", u8 ("Lautst\xc3\xa4rke")) });
    finishSetup();
}

void AlmEditor::layoutBody (juce::Rectangle<int> body)
{
    const int rowH = body.getHeight() / 3;
    layoutSection ("INSTRUMENT", body.removeFromTop (rowH));
    auto r2 = split (body.removeFromTop (rowH), { 5, 2 });
    layoutSection ("KLANG", r2[0]);
    layoutSection ("JODLER", r2[1]);
    // unten rechts bleibt ein Fenster auf die Alm
    auto r3 = split (body, { 3, 1, 3 });
    layoutSection ("BERG", r3[0]);
    layoutSection ("AUSGANG", r3[1]);
}

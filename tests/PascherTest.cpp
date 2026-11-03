#include "TestUtil.h"
#include "../plugins/pascher/Source/PluginProcessor.h"
#include "../plugins/pascher/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    PascherProcessor proc;

    juce::AudioBuffer<float> tone (2, (int) (sr * 6));
    for (int i = 0; i < tone.getNumSamples(); ++i) { const float v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / sr); tone.setSample (0, i, v); tone.setSample (1, i, v); }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto out = tt::run (proc, tone, sr);
        tt::check (tt::finite (out) && tt::peak (out, 0, out.getNumSamples()) < 1.0f && tt::peak (out, 0, out.getNumSamples()) > 0.05f,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Spitze=%.3f", tt::peak (out, 0, out.getNumSamples())));
    }

    // Gate: Ländler-Gate (1/8 bei 120 BPM = 250 ms je Schritt; an: 0,2,4…, Tiefe 0.85)
    proc.setCurrentProgram (3);
    proc.apvts.getParameter ("swing")->setValueNotifyingHost (0.0f);
    {
        auto out = tt::run (proc, tone, sr);
        const int step = (int) (0.25 * sr);
        const float on  = out.getRMSLevel (0, 2 * step + step / 4, step / 2);     // Schritt 2: an
        const float off = out.getRMSLevel (0, 3 * step + step / 4, step / 2);     // Schritt 3: aus
        tt::check (on > 0.18f && off < on * 0.25f, juce::String::formatted ("Gate: an=%.3f  aus=%.3f (Tiefe 85 %%)", on, off));
    }

    // Klatschen kommt auf den Schritten: ein Klatscher, kein "Mensch", Muster x.xxx.x.xxx.
    proc.setCurrentProgram (0);
    for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "klatscher", 1.0f }, { "mensch", 0.0f }, { "swing", 0.0f }, { "raum", 0.0f } })
        proc.apvts.getParameter (id)->setValueNotifyingHost (proc.apvts.getParameter (id)->convertTo0to1 (v));
    {
        juce::AudioBuffer<float> silence (2, (int) (sr * 3.1));
        silence.clear();
        auto out = tt::run (proc, silence, sr);
        const char* pat = "x.xxx.x.xxx.";
        bool ok = true; juce::String found;
        const int step = (int) (0.25 * sr);
        for (int s = 0; s < 12; ++s)
        {
            const float p = tt::peak (out, s * step, step / 3);
            found << (p > 0.05f ? "x" : ".");
            ok = ok && ((pat[s] == 'x') == (p > 0.05f));
        }
        tt::check (ok, "Klatsch-Muster erkannt: " + found + "  (soll x.xxx.x.xxx.)");
    }

    // Hörprobe: Plucks durch Gate + Klatschen
    {
        auto pl = tt::plucks (sr, 6.0, 6.0);
        juce::AudioBuffer<float> demo (2, pl.getNumSamples() * 3);
        demo.clear();
        int at = 0;
        for (int pi : { 0, 2, 4 })
        {
            proc.setCurrentProgram (pi);
            auto out = tt::run (proc, pi == 4 ? tone : pl, sr);
            for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, at, out, ch, 0, pl.getNumSamples());
            at += pl.getNumSamples();
        }
        tt::writeWav (dir.getChildFile ("pascher_demo.wav"), demo, sr);
    }

    proc.setCurrentProgram (0);
    tt::run (proc, tone, sr);
    tt::waitForTexture ("pascher", tmk::tex::landscapeFor ("pascher"), dir);
    tt::screenshots<PascherEditor> (proc, dir, "pascher", 1);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

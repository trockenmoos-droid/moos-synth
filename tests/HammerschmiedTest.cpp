#include "TestUtil.h"
#include "../plugins/hammerschmied/Source/PluginProcessor.h"
#include "../plugins/hammerschmied/Source/PluginEditor.h"

static juce::AudioBuffer<float> zeros (int n) { juce::AudioBuffer<float> b (2, n); b.clear(); return b; }

static void setP (juce::AudioProcessor& p, const juce::String& id, float v)
{
    auto* a = dynamic_cast<HammerProcessor&> (p).apvts.getParameter (id);
    a->setValueNotifyingHost (a->convertTo0to1 (v));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    HammerProcessor proc;

    // Euklid: E(3,8) = x..x..x.
    {
        juce::String s; for (int i = 0; i < 8; ++i) s << (hammer::euclid (i, 8, 3, 0) ? "x" : ".");
        int n5 = 0; for (int i = 0; i < 13; ++i) n5 += hammer::euclid (i, 13, 5, 3) ? 1 : 0;
        tt::check (s == "x..x..x." && n5 == 5, "Euklid E(3,8) = " + s + ", E(5,13) hat " + juce::String (n5) + " Schläge");
    }

    juce::AudioBuffer<float> silence (2, (int) (sr * 8));
    silence.clear();
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto out = tt::run (proc, silence, sr);
        const float pk = tt::peak (out, 0, out.getNumSamples());
        const float rms = out.getRMSLevel (0, 0, out.getNumSamples());
        tt::check (tt::finite (out) && pk <= 1.0f && pk > 0.1f && rms > 0.01f,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Spitze=%.3f  RMS=%.3f", pk, rms));
    }

    // Ohne Frei-Lauf und ohne Transport: still – MIDI-Noten 36..41 schlagen die Spuren an
    proc.setCurrentProgram (0);
    setP (proc, "frei", 0.0f); setP (proc, "glut", 0.0f);
    {
        auto out = tt::run (proc, zeros ((int) sr), sr);
        tt::check (tt::peak (out, 0, out.getNumSamples()) < 1.0e-4f, juce::String::formatted ("Ohne Transport und ohne Frei: Stille (Spitze %.5f bei %d)", tt::peak (out, 0, out.getNumSamples()), [&] { int k = 0; for (int i = 0; i < out.getNumSamples(); ++i) if (std::abs (out.getSample (0, i)) > 1.0e-4f) { k = i; break; } return k; }()));
        juce::AudioBuffer<float> in (2, (int) (sr * 3)); in.clear();
        auto hit = tt::run (proc, in, sr, [&] (int pos, juce::MidiBuffer& m)
        {
            for (int t = 0; t < 6; ++t)
            {
                const int at = (int) (t * 0.4 * sr);
                if (at >= pos && at < pos + 512) m.addEvent (juce::MidiMessage::noteOn (10, 36 + t, (juce::uint8) 110), at - pos);
            }
        });
        juce::String peaks; bool ok = true;
        for (int t = 0; t < 6; ++t)
        {
            const float p = tt::peak (hit, (int) (t * 0.4 * sr), (int) (0.1 * sr));
            peaks << juce::String::formatted ("%.2f ", p);
            ok = ok && p > 0.03f;
        }
        tt::check (ok, "MIDI 36-41 spielt alle sechs Spuren: " + peaks);
    }

    // Glut verändert den Klang (Granular + Stotter mischt sich hinein)
    {
        proc.setCurrentProgram (5);  // Glitch-Geselle
        setP (proc, "glut", 0.0f);
        auto dry = tt::run (proc, silence, sr);
        proc.setCurrentProgram (5);
        auto wet = tt::run (proc, silence, sr);
        double diff = 0.0;
        for (int i = 0; i < dry.getNumSamples(); ++i) diff += std::abs (dry.getSample (0, i) - wet.getSample (0, i));
        tt::check (diff / dry.getNumSamples() > 0.005, juce::String::formatted ("Glut wirkt: mittlere Abweichung %.4f", diff / dry.getNumSamples()));
    }

    // Hörprobe: alle Presets je 6 Sekunden
    {
        const int seg = (int) (sr * 6);
        juce::AudioBuffer<float> demo (2, seg * proc.getNumPrograms());
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            auto out = tt::run (proc, zeros (seg), sr);
            for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, i * seg, out, ch, 0, seg);
        }
        tt::writeWav (dir.getChildFile ("hammerschmied_demo.wav"), demo, sr);
    }

    proc.setCurrentProgram (0);
    tt::run (proc, silence, sr);
    tt::waitForTexture ("hammerschmied", tmk::tex::landscapeFor ("hammerschmied"), dir);
    tt::screenshots<HammerEditor> (proc, dir, "hammerschmied", 1);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

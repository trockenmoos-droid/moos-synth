#include "TestUtil.h"
#include "../plugins/sarstein/Source/PluginProcessor.h"
#include "../plugins/sarstein/Source/PluginEditor.h"

static double goertzel (const juce::AudioBuffer<float>& b, int start, int num, double freq, double sr)
{
    const double w = juce::MathConstants<double>::twoPi * freq / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < num; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (num - 1));
        const double s0 = b.getSample (0, start + i) * win + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / num;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();

    SarsteinProcessor proc;
    auto sine = [&] (double f, float amp, double secs)
    {
        juce::AudioBuffer<float> b (2, (int) (secs * sr));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * f * i / sr);
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        return b;
    };
    auto rms = [] (const juce::AudioBuffer<float>& b, int s, int n) { return b.getRMSLevel (0, s, n); };

    auto in = sine (220.0, 0.3f, 2.0);
    const float inRms = rms (in, 0, in.getNumSamples());
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto out = tt::run (proc, in, sr);
        const float o = rms (out, (int) sr / 2, (int) sr);
        const float diffDb = juce::Decibels::gainToDecibels (o / inRms);
        tt::check (tt::finite (out) && std::abs (diffDb) < 9.0f && tt::peak (out, 0, out.getNumSamples()) < 1.5f,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Pegel %+.1f dB gegenueber Eingang", diffDb));
    }

    // Aliasing: 7 kHz, Granit, 40 dB Drive -> gespiegelte Obertöne (13 kHz, 1 kHz) müssen leise bleiben
    proc.setCurrentProgram (0);
    proc.apvts.getParameter ("stein")->setValueNotifyingHost (proc.apvts.getParameter ("stein")->convertTo0to1 (1));
    proc.apvts.getParameter ("drive")->setValueNotifyingHost (proc.apvts.getParameter ("drive")->convertTo0to1 (40.0f));
    proc.apvts.getParameter ("highcut")->setValueNotifyingHost (1.0f);
    {
        auto hi = sine (7000.0, 0.5f, 1.0);
        auto out = tt::run (proc, hi, sr);
        const double fund = goertzel (out, 8192, 32768, 7000.0, sr);
        const double alias1 = goertzel (out, 8192, 32768, 13000.0, sr);   // 5. Oberton (35 kHz) gespiegelt
        const double alias2 = goertzel (out, 8192, 32768, 1000.0, sr);    // 7. Oberton (49 kHz) gespiegelt
        const double worst = juce::Decibels::gainToDecibels ((float) (juce::jmax (alias1, alias2) / fund));
        tt::check (worst < -35.0, juce::String::formatted ("Aliasing bei 7 kHz / 40 dB: %.1f dB unter dem Grundton", -worst));
    }

    // Mix 0 = unverändert (bis auf die Latenz)
    proc.setCurrentProgram (0);
    proc.apvts.getParameter ("mix")->setValueNotifyingHost (0.0f);
    proc.apvts.getParameter ("autogain")->setValueNotifyingHost (0.0f);
    {
        auto pl = tt::plucks (sr, 2.0, 1.5);
        auto out = tt::run (proc, pl, sr);
        const int lat = proc.getLatencySamples();
        float maxDiff = 0.0f;
        for (int i = 4000; i < pl.getNumSamples() - 4000; ++i) maxDiff = juce::jmax (maxDiff, std::abs (out.getSample (0, i) - pl.getSample (0, i - lat)));
        tt::check (maxDiff < 1.0e-4f, juce::String::formatted ("Mix 0 ist bitgenau trocken (Latenz %d Samples, Abweichung %.6f)", lat, maxDiff));
    }

    // Hörprobe: Plucks durch 5 Gesteine
    {
        auto dry = tt::plucks (sr, 4.0, 3.2);
        const int presets[] = { 0, 1, 2, 3, 4 };
        juce::AudioBuffer<float> demo (2, dry.getNumSamples() * 6);
        demo.clear();
        for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, 0, dry, ch, 0, dry.getNumSamples());
        int at = dry.getNumSamples();
        for (int pi : presets)
        {
            proc.setCurrentProgram (pi);
            auto out = tt::run (proc, dry, sr);
            for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, at, out, ch, 0, out.getNumSamples());
            at += out.getNumSamples();
        }
        tt::writeWav (dir.getChildFile ("sarstein_demo.wav"), demo, sr);
    }

    proc.setCurrentProgram (5);
    tt::run (proc, tt::plucks (sr, 1.0, 0.5), sr);
    tt::waitForTexture ("sarstein", [] { return tmk::tex::limestone(); }, dir);
    tt::screenshots<SarsteinEditor> (proc, dir, "sarstein", 1);

    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

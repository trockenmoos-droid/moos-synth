#include "TestUtil.h"
#include "../plugins/5fingers/Source/PluginProcessor.h"
#include "../plugins/5fingers/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();

    FingersProcessor proc;

    juce::AudioBuffer<float> click (2, (int) (sr * 12));
    click.clear();
    for (int i = 0; i < 300; ++i) { const float v = 0.6f * std::sin (i * 0.25f) * (1.0f - i / 300.0f); click.setSample (0, i, v); click.setSample (1, i, v); }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto pl = tt::plucks (sr, 12.0, 1.5);
        auto out = tt::run (proc, pl, sr);
        const float mid  = tt::peak (out, (int) (sr * 2), (int) (sr * 2));
        const float late = tt::peak (out, (int) (sr * 11), (int) (sr * 0.9));
        tt::check (tt::finite (out) && mid > 0.01f && tt::peak (out, 0, out.getNumSamples()) < 1.6f && late < mid,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Echos=%.3f  nach 11 s=%.4f", mid, late));
    }

    // Fünf Echos zu den richtigen Zeiten (sync aus, 1000 ms, Körnung 0)
    proc.setCurrentProgram (0);
    for (auto [pid, val] : std::vector<std::pair<const char*, float>> { { "sync", 0.0f }, { "time", 1000.0f }, { "feedback", 0.0f }, { "mix", 1.0f } })
        proc.apvts.getParameter (pid)->setValueNotifyingHost (proc.apvts.getParameter (pid)->convertTo0to1 (val));
    {
        auto out = tt::run (proc, click, sr);
        bool ok = true;
        juce::String found;
        for (int f = 0; f < 5; ++f)
        {
            const int expect = (int) ((0.2 * (f + 1)) * sr);
            const float p = tt::peak (out, expect - 200, 1200);
            const float between = tt::peak (out, expect + 2400, 3000);
            found << juce::String::formatted (" %d:%.2f", f + 1, p);
            ok = ok && p > 0.05f && between < 0.01f;
        }
        tt::check (ok, "Fuenf Echos bei 200/400/600/800/1000 ms:" + found);
    }

    // Granular: Tonhöhe +12 verschiebt das Spektrum (Nulldurchgänge zählen)
    {
        juce::AudioBuffer<float> tone (2, (int) (sr * 4));
        for (int i = 0; i < tone.getNumSamples(); ++i) { const float v = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 300.0 * i / sr); tone.setSample (0, i, v); tone.setSample (1, i, v); }
        auto measure = [&] (int semis)
        {
            proc.setCurrentProgram (0);
            for (int f = 0; f < 5; ++f) { proc.apvts.getParameter (fingers::id ("korn", f))->setValueNotifyingHost (f == 4 ? 1.0f : 0.0f);
                                          proc.apvts.getParameter (fingers::id ("pegel", f))->setValueNotifyingHost (f == 4 ? 1.0f : 0.0f); }
            for (auto [pid, val] : std::vector<std::pair<const char*, float>> { { "sync", 0.0f }, { "time", 500.0f }, { "feedback", 0.0f }, { "mix", 1.0f },
                                                                              { "streuung", 0.0f }, { "tonhoehe", (float) semis }, { "korngroesse", 100.0f } })
                proc.apvts.getParameter (pid)->setValueNotifyingHost (proc.apvts.getParameter (pid)->convertTo0to1 (val));
            auto out = tt::run (proc, tone, sr);
            int zc = 0;
            for (int i = (int) sr * 2 + 1; i < (int) sr * 3; ++i) if ((out.getSample (0, i - 1) < 0) != (out.getSample (0, i) < 0)) ++zc;
            return zc / 2.0;   // ~ Frequenz in Hz
        };
        const double f0 = measure (0), f12 = measure (12), fm12 = measure (-12);
        tt::check (std::abs (f0 - 300) < 30 && std::abs (f12 - 600) < 60 && std::abs (fm12 - 150) < 25,
                   juce::String::formatted ("Granular-Tonhoehe: 0 HT = %.0f Hz, +12 = %.0f Hz, -12 = %.0f Hz (soll 300/600/150)", f0, f12, fm12));
    }

    // Stress: alles auf Maximum
    proc.setCurrentProgram (0);
    for (auto pid : { "feedback", "dichte", "streuung", "rueckwaerts", "mix", "korn1", "korn2", "korn3", "korn4", "korn5" })
        proc.apvts.getParameter (pid)->setValueNotifyingHost (1.0f);
    {
        auto pl = tt::plucks (sr, 15.0, 3.0);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = tt::run (proc, pl, sr);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        tt::check (tt::finite (out) && tt::peak (out, 0, out.getNumSamples()) < 1.6f,
                   juce::String::formatted ("Maximum: Spitze=%.3f  Rechenzeit=%.1f%% Echtzeit", tt::peak (out, 0, out.getNumSamples()), ms / 150.0));
    }

    // Festhalten: die Aussicht bleibt stehen, auch ohne Eingang und ohne Feedback – und wächst nicht
    {
        auto hold = [&] (bool on)
        {
            proc.setCurrentProgram (0);
            auto setv = [&] (const char* id, float v) { auto* a = proc.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v)); };
            setv ("feedback", 0.0f); setv ("mix", 1.0f); setv ("korn3", 0.8f);
            auto in = tt::plucks (sr, 14.0, 2.0);
            for (int ch = 0; ch < 2; ++ch) in.clear (ch, (int) (sr * 2.2), in.getNumSamples() - (int) (sr * 2.2));
            proc.setPlayConfigDetails (2, 2, sr, 512);
            proc.prepareToPlay (sr, 512);
            juce::AudioBuffer<float> out (2, in.getNumSamples()), b (2, 512);
            for (int pos = 0; pos + 512 <= in.getNumSamples(); pos += 512)
            {
                if (pos >= (int) (sr * 1.8) && pos < (int) (sr * 1.8) + 512) setv ("festhalten", on ? 1.0f : 0.0f);
                for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, in, ch, pos, 512);
                juce::MidiBuffer m; proc.processBlock (b, m);
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, 512);
            }
            setv ("festhalten", 0.0f);
            return std::pair<float, float> { out.getRMSLevel (0, (int) (sr * 5), (int) (sr * 2)), out.getRMSLevel (0, (int) (sr * 11), (int) (sr * 2)) };
        };
        auto [a0, b0] = hold (false);
        auto [a1, b1] = hold (true);
        tt::check (b0 < 0.001f && a1 > 0.01f && b1 > a1 * 0.5f && b1 < a1 * 1.5f,
                   juce::String::formatted ("Festhalten: RMS 5 s=%.3f, 11 s=%.3f (ohne: %.4f)", a1, b1, b0));
    }

    // Hörprobe
    {
        auto dry = tt::plucks (sr, 7.0, 2.4);
        const int presets[] = { 0, 1, 2, 4, 6 };
        juce::AudioBuffer<float> demo (2, dry.getNumSamples() * 5);
        demo.clear();
        int at = 0;
        for (int pi : presets)
        {
            proc.setCurrentProgram (pi);
            auto out = tt::run (proc, dry, sr);
            for (int ch = 0; ch < 2; ++ch) { demo.copyFrom (ch, at, out, ch, 0, out.getNumSamples()); demo.applyGainRamp (ch, at + out.getNumSamples() - 12000, 12000, 1.0f, 0.0f); }
            at += out.getNumSamples();
        }
        tt::writeWav (dir.getChildFile ("5fingers_demo.wav"), demo, sr);
    }

    proc.setCurrentProgram (1);
    tt::run (proc, tt::plucks (sr, 2.0, 1.5), sr);
    tt::waitForTexture ("5fingers", [] { return tmk::tex::cloudSea(); }, dir);
    tt::screenshots<FingersEditor> (proc, dir, "5fingers", 1);

    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

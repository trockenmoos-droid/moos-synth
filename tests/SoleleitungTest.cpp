#include "TestUtil.h"
#include "../plugins/soleleitung/Source/PluginProcessor.h"
#include "../plugins/soleleitung/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();

    SoleleitungProcessor proc;

    // Klick bei 0, dann Stille: Echos müssen kommen (und bei Feedback < 1 abklingen)
    juce::AudioBuffer<float> click (2, (int) (sr * 14));
    click.clear();
    for (int i = 0; i < 200; ++i) { const float v = 0.6f * std::sin (i * 0.3f) * (1.0f - i / 200.0f); click.setSample (0, i, v); click.setSample (1, i, v); }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        proc.apvts.getParameter ("mix")->setValueNotifyingHost (0.5f);
        auto out = tt::run (proc, click, sr);
        const float echo = tt::peak (out, (int) (sr * 0.05), (int) (sr * 2.5));
        const float late = tt::peak (out, (int) (sr * 12.5), (int) (sr * 1.4));
        const float fbv  = proc.apvts.getRawParameterValue ("feedback")->load();
        const bool ok = tt::finite (out) && echo > 0.02f && late < 1.01f && (fbv >= 1.0f || late < echo * 0.5f);
        tt::check (ok, "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Echo=%.3f  nach 12.5 s=%.4f  (fb %.2f)", echo, late, fbv));
    }

    // Erstes Echo kommt zur richtigen Zeit (sync aus, 250 ms, keine Strömung)
    proc.setCurrentProgram (7);
    for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "sync", 0.0f }, { "time", 250.0f }, { "stroemung", 0.0f }, { "mix", 1.0f }, { "breite", 0.0f } })
        proc.apvts.getParameter (id)->setValueNotifyingHost (proc.apvts.getParameter (id)->convertTo0to1 (v));
    {
        auto out = tt::run (proc, click, sr);
        int first = -1;
        for (int i = 2000; i < out.getNumSamples() && first < 0; ++i) if (std::abs (out.getSample (0, i)) > 0.05f) first = i;
        const double msFound = first * 1000.0 / sr;
        tt::check (std::abs (msFound - 250.0) < 3.0, juce::String::formatted ("Erstes Echo bei %.1f ms (soll 250)", msFound));
    }

    // Extremwerte: Feedback max, Rohr max, Salz max -> begrenzt, keine NaN
    proc.setCurrentProgram (0);
    for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "feedback", 1.0f }, { "rohr", 1.0f }, { "salz", 1.0f }, { "stroemung", 1.0f }, { "mix", 1.0f }, { "stations", 1.0f } })
        proc.apvts.getParameter (id)->setValueNotifyingHost (v);
    {
        auto out = tt::run (proc, click, sr);
        tt::check (tt::finite (out) && tt::peak (out, 0, out.getNumSamples()) <= 1.01f,
                   juce::String::formatted ("Extremwerte begrenzt, Spitze=%.3f", tt::peak (out, 0, out.getNumSamples())));
    }

    // Absperren hält die Schleife
    proc.setCurrentProgram (0);
    proc.apvts.getParameter ("mix")->setValueNotifyingHost (1.0f);
    {
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> b (2, 512);
        float at3 = 0, at12 = 0;
        auto dry = tt::plucks (sr, 14.0, 1.0);
        for (int pos = 0; pos + 512 <= dry.getNumSamples(); pos += 512)
        {
            if (pos >= (int) (sr * 1.0) && pos < (int) (sr * 1.0) + 512) proc.apvts.getParameter ("absperren")->setValueNotifyingHost (1.0f);
            for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, dry, ch, pos, 512);
            juce::MidiBuffer m;
            proc.processBlock (b, m);
            if (pos > sr * 2 && pos < sr * 4) at3 = juce::jmax (at3, b.getMagnitude (0, 512));
            if (pos > sr * 11) at12 = juce::jmax (at12, b.getMagnitude (0, 512));
        }
        tt::check (at12 > at3 * 0.5f && at12 < at3 * 1.5f, juce::String::formatted ("Absperren haelt: 3 s=%.3f  12 s=%.3f", at3, at12));
        proc.apvts.getParameter ("absperren")->setValueNotifyingHost (0.0f);
    }

    // Hörprobe
    {
        auto dry = tt::plucks (sr, 7.0, 2.4);
        const int presets[] = { 0, 1, 2, 3, 6 };
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
        tt::writeWav (dir.getChildFile ("soleleitung_demo.wav"), demo, sr);
    }

    proc.setCurrentProgram (1);
    {
        auto dry = tt::plucks (sr, 2.0, 1.0);
        tt::run (proc, dry, sr);     // Pegel für die Anzeige
    }
    tt::waitForTexture ("soleleitung", [] { return tmk::tex::woodPipe(); }, dir);
    tt::screenshots<SoleleitungEditor> (proc, dir, "soleleitung", 1);

    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

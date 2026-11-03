#include "TestUtil.h"
#include "../plugins/hallstatt/Source/PluginProcessor.h"
#include "../plugins/hallstatt/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();

    HallstattProcessor proc;

    // Rauschimpuls 0.3 s, dann Stille
    juce::AudioBuffer<float> burst (2, (int) (sr * 12));
    burst.clear();
    juce::Random rng (3);
    for (int i = 0; i < (int) (sr * 0.3); ++i) { burst.setSample (0, i, (rng.nextFloat() - 0.5f) * 0.6f); burst.setSample (1, i, (rng.nextFloat() - 0.5f) * 0.6f); }

    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        proc.apvts.getParameter ("mix")->setValueNotifyingHost (1.0f);         // nur Hall
        auto out = tt::run (proc, burst, sr);
        const float early = tt::peak (out, (int) (sr * 0.3), (int) (sr * 0.7));
        const float late  = tt::peak (out, (int) (sr * 11), (int) (sr * 0.9));
        const float dec   = proc.apvts.getRawParameterValue ("decay")->load();
        const bool decays = dec > 15.0f ? late < early : late < early * 0.2f;
        tt::check (tt::finite (out) && early > 0.01f && early < 4.0f && decays,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  hall=%.3f  nach 11 s=%.4f", early, late));
    }

    // Extremwerte: Bass 2, Dämpfung 1, Nachhall 30 s, Größe max
    proc.setCurrentProgram (0);
    for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "bass", 1.0f }, { "damping", 1.0f }, { "decay", 1.0f }, { "size", 1.0f }, { "mix", 1.0f }, { "mod", 1.0f } })
        proc.apvts.getParameter (id)->setValueNotifyingHost (v);
    for (int room = 0; room < 5; ++room)
    {
        proc.apvts.getParameter ("room")->setValueNotifyingHost (room / 4.0f);
        auto out = tt::run (proc, burst, sr);
        const float a = tt::peak (out, (int) (sr * 1), (int) (sr * 1)), b = tt::peak (out, (int) (sr * 10), (int) (sr * 1.5));
        tt::check (tt::finite (out) && b <= a * 1.05f, "Extremwerte Raum " + juce::String (room) + juce::String::formatted ("  1 s=%.3f  10 s=%.3f", a, b));
    }

    // Freeze: hält das Signal, wächst nicht
    proc.setCurrentProgram (4);
    proc.apvts.getParameter ("mix")->setValueNotifyingHost (1.0f);
    {
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> buf (2, 512);
        float at2 = 0, at11 = 0;
        for (int pos = 0; pos + 512 <= burst.getNumSamples(); pos += 512)
        {
            if (pos >= (int) (sr * 0.8) && pos < (int) (sr * 0.8) + 512) proc.apvts.getParameter ("freeze")->setValueNotifyingHost (1.0f);
            for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, burst, ch, pos, 512);
            juce::MidiBuffer m;
            proc.processBlock (buf, m);
            if (pos > sr * 2 && pos < sr * 3) at2 = juce::jmax (at2, buf.getMagnitude (0, 512));
            if (pos > sr * 10.5) at11 = juce::jmax (at11, buf.getMagnitude (0, 512));
        }
        tt::check (at11 > at2 * 0.3f && at11 < at2 * 1.5f, juce::String::formatted ("Freeze haelt: 2 s=%.3f  11 s=%.3f", at2, at11));
        proc.apvts.getParameter ("freeze")->setValueNotifyingHost (0.0f);
    }

    // Hörprobe: Plucks trocken, dann durch 4 Räume
    {
        auto dry = tt::plucks (sr, 7.0, 3.2);
        const int presets[] = { 0, 2, 3, 4 };
        juce::AudioBuffer<float> demo (2, dry.getNumSamples() * 5);
        demo.clear();
        for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, 0, dry, ch, 0, dry.getNumSamples());
        int at = dry.getNumSamples();
        for (int pi : presets)
        {
            proc.setCurrentProgram (pi);
            auto out = tt::run (proc, dry, sr);
            for (int ch = 0; ch < 2; ++ch) { demo.copyFrom (ch, at, out, ch, 0, out.getNumSamples()); demo.applyGainRamp (ch, at + out.getNumSamples() - 12000, 12000, 1.0f, 0.0f); }
            at += out.getNumSamples();
        }
        tt::writeWav (dir.getChildFile ("hallstatt_demo.wav"), demo, sr);
    }

    proc.setCurrentProgram (3);
    tt::waitForTexture ("hallstatt", [] { return tmk::tex::lakeAtDusk(); }, dir);
    tt::screenshots<HallstattEditor> (proc, dir, "hallstatt", 1);

    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

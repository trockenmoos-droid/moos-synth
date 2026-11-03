#include "TestUtil.h"
#include "../plugins/dachstein/Source/PluginProcessor.h"
#include "../plugins/dachstein/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();

    DachsteinProcessor proc;
    const std::vector<int> chord { 41, 48, 55, 60, 64, 67 };   // F-Dur add9-artig, weit gelegt

    auto play = [&] (double seconds, double releaseAt, std::vector<juce::MidiMessage> extra = {})
    {
        juce::AudioBuffer<float> silence (2, (int) (seconds * sr));
        silence.clear();
        return tt::run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m)
        {
            if (pos == 0) { for (int nn : chord) m.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0); for (auto& e : extra) m.addEvent (e, 1); }
            const int rel = (int) (releaseAt * sr);
            if (pos <= rel && pos + 512 > rel) for (int nn : chord) m.addEvent (juce::MidiMessage::noteOff (1, nn), rel - pos);
        });
    };

    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = play (8.0, 5.0, { juce::MidiMessage::controllerEvent (1, 1, 80), juce::MidiMessage::channelPressureChange (1, 70) });
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const float lvl = tt::peak (out, (int) (sr * 3), (int) (sr * 2));
        tt::check (tt::finite (out) && lvl > 0.02f && lvl < 0.99f,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Pegel=%.3f  Rechenzeit=%.0f%% Echtzeit (6 Stimmen)", lvl, ms / 80.0));
    }

    // Alles ausklingen? (kurzer Release, kein Hall)
    proc.setCurrentProgram (0);
    proc.apvts.getParameter ("release")->setValueNotifyingHost (0.0f);
    proc.apvts.getParameter ("hall")->setValueNotifyingHost (0.0f);
    {
        auto out = play (6.0, 2.0);
        tt::check (tt::peak (out, (int) (sr * 5), (int) (sr * 0.9)) < 0.001f, "Klingt nach Release aus");
    }

    // 8 Stimmen, alles auf Maximum
    proc.setCurrentProgram (0);
    for (auto id : { "obertoene", "kristall", "teiltoene", "bewegung", "schmelze", "fels", "wind", "knistern", "hall", "volume" })
        proc.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
    {
        juce::AudioBuffer<float> silence (2, (int) (6 * sr));
        silence.clear();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = tt::run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m)
        {
            if (pos == 0) for (int nn = 36; nn < 36 + 8 * 7; nn += 7) m.addEvent (juce::MidiMessage::noteOn (1, nn, 1.0f), 0);
        });
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        tt::check (tt::finite (out) && tt::peak (out, 0, out.getNumSamples()) <= 1.0f,
                   juce::String::formatted ("Maximum, 8 Stimmen: Spitze=%.3f  Rechenzeit=%.0f%% Echtzeit", tt::peak (out, 0, out.getNumSamples()), ms / 60.0));
    }

    // Hörprobe
    {
        const int presets[] = { 0, 2, 3, 5, 7 };
        juce::AudioBuffer<float> demo (2, (int) (sr * 9) * 5);
        demo.clear();
        int at = 0;
        for (int pi : presets)
        {
            proc.setCurrentProgram (pi);
            proc.apvts.getParameter ("attack")->setValueNotifyingHost (proc.apvts.getParameter ("attack")->convertTo0to1 (
                juce::jmin (2.0f, proc.apvts.getRawParameterValue ("attack")->load())));
            auto out = play (9.0, 5.5);
            for (int ch = 0; ch < 2; ++ch) { demo.copyFrom (ch, at, out, ch, 0, out.getNumSamples()); demo.applyGainRamp (ch, at + out.getNumSamples() - 24000, 24000, 1.0f, 0.0f); }
            at += out.getNumSamples();
        }
        tt::writeWav (dir.getChildFile ("dachstein_demo.wav"), demo, sr);
    }

    // Für den Screenshot: Akkord gehalten
    proc.setCurrentProgram (1);
    {
        juce::AudioBuffer<float> silence (2, (int) (3 * sr));
        silence.clear();
        tt::run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m) { if (pos == 0) for (int nn : chord) m.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0); });
    }
    tt::waitForTexture ("dachstein", [] { return tmk::tex::glacierIce(); }, dir);
    tt::screenshots<DachsteinEditor> (proc, dir, "dachstein", 1);

    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

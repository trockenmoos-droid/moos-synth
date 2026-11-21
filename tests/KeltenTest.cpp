#include "TestUtil.h"
#include "../plugins/kelten/Source/PluginProcessor.h"
#include "../plugins/kelten/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    KeltenProcessor proc;
    // Heer: ein einzelner Ton in der Mitte wird breit (Seitenanteil steigt), Pegel bleibt ähnlich
    {
        const double sr = 48000.0;
        auto set = [&] (const char* id, float v) { auto* a = proc.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        auto measure = [&] (float h)
        {
            proc.setCurrentProgram (0);
            set ("heer", h); set ("hall", 0.0f); set ("bordun", 0.0f);
            juce::AudioBuffer<float> silence (2, (int) (sr * 3)); silence.clear();
            auto out = tt::run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 55, 0.8f), 0); });
            double mid = 0, side = 0;
            for (int i = (int) sr; i < (int) (sr * 2.5); ++i) { const double l = out.getSample (0, i), r = out.getSample (1, i); mid += (l + r) * (l + r); side += (l - r) * (l - r); }
            return std::tuple<double, float, bool> { side / juce::jmax (1.0e-9, mid), tt::peak (out, 0, out.getNumSamples()), tt::finite (out) };
        };
        auto [s0, p0, f0] = measure (0.0f);
        auto [s1, p1, f1] = measure (1.0f);
        tt::check (f0 && f1 && s1 > s0 + 0.05 && p1 < 1.0f && p1 > p0 * 0.6f,
                   juce::String::formatted ("Heer: Seitenanteil %.3f -> %.3f, Spitze %.3f -> %.3f", s0, s1, p0, p1));
        set ("heer", 0.0f);
    }
    tt::synthSuite<KeltenEditor> (proc, dir, "kelten", "kelten", { 0, 1, 2, 3, 5 }, { 50, 57, 62 }, 3.5, 6.0);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

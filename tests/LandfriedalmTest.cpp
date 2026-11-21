#include "TestUtil.h"
#include "../plugins/landfriedalm/Source/PluginProcessor.h"
#include "../plugins/landfriedalm/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    AlmProcessor proc;
    // Wirbel: ein gehaltener Ton wird im Takt wiederholt (1/16 bei 120 BPM = 8 Schläge/s), der Jodler nicht
    {
        const double sr = 48000.0;
        auto set = [&] (const char* id, float v) { auto* a = proc.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        auto roll = [&] (int inst)
        {
            proc.setCurrentProgram (0);
            set ("instrument", (float) inst); set ("wirbel", 2.0f); set ("wirbelFrei", 0.0f);
            juce::AudioBuffer<float> silence (2, (int) (sr * 3)); silence.clear();
            const int before = proc.rollStrikes.load();
            auto out = tt::run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m)
            {
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 62, 0.8f), 0);
                if (pos <= (int) (sr * 2) && pos + 512 > (int) (sr * 2)) m.addEvent (juce::MidiMessage::noteOff (1, 62), (int) (sr * 2) - pos);
            });
            return std::pair<int, float> { proc.rollStrikes.load() - before, tt::peak (out, 0, out.getNumSamples()) };
        };
        auto [hb, hp] = roll (1);
        auto [jo, jp] = roll (3);
        tt::check (hb >= 14 && hb <= 17 && hp < 1.0f && hp > 0.05f && jo == 0,
                   juce::String::formatted ("Wirbel: Hackbrett %d Schlaege in 2 s (Spitze %.2f), Jodler %d", hb, hp, jo));
        set ("wirbel", 0.0f);
    }
    tt::synthSuite<AlmEditor> (proc, dir, "landfriedalm", "landfriedalm", { 0, 2, 4, 6, 7 }, { 55, 62, 67, 71 }, 3.0, 5.0);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

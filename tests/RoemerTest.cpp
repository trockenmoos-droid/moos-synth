#include "TestUtil.h"
#include "../plugins/roemer/Source/PluginProcessor.h"
#include "../plugins/roemer/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    RoemerProcessor proc;
    // Signal: eine gehaltene Taste ergibt Naturton-Rufe im Takt (Achtel bei 120 BPM = 4 Töne/s)
    {
        const double sr = 48000.0;
        auto set = [&] (const char* id, float v) { auto* a = proc.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        auto calls = [&] (int mode)
        {
            proc.setCurrentProgram (0);
            set ("cornu", 1.0f); set ("signal", (float) mode); set ("signalTakt", 1.0f);
            juce::AudioBuffer<float> silence (2, (int) (sr * 3.5)); silence.clear();
            const int before = proc.signalNotes.load();
            auto out = tt::run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m)
            {
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                if (pos <= (int) (sr * 3) && pos + 512 > (int) (sr * 3)) m.addEvent (juce::MidiMessage::noteOff (1, 60), (int) (sr * 3) - pos);
            });
            return std::pair<int, float> { proc.signalNotes.load() - before, tt::peak (out, 0, out.getNumSamples()) };
        };
        auto [s, sp] = calls (1);
        auto [f, fp] = calls (2);
        tt::check (s >= 11 && s <= 13 && f >= 9 && f < s && sp > 0.02f && sp < 1.0f,
                   juce::String::formatted ("Signal: %d Rufe, Fanfare %d (mit Pausen), Spitze %.2f", s, f, sp));
        set ("signal", 0.0f);
    }
    tt::synthSuite<RoemerEditor> (proc, dir, "roemer", "roemer", { 0, 1, 2, 4, 5 }, { 53, 60, 65, 69 }, 3.5, 6.0);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

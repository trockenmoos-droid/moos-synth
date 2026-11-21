#include "TestUtil.h"
#include "../plugins/sanktwolfgang/Source/PluginProcessor.h"
#include "../plugins/sanktwolfgang/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    WolfgangProcessor proc;
    // Geläut: vier gehaltene Töne werden einzeln nacheinander angeschlagen (Plain Hunt), im Takt
    {
        const double sr = 48000.0;
        proc.setCurrentProgram (0);
        auto set = [&] (const char* id, float v) { auto* a = proc.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        set ("gelaeut", 2.0f); set ("schlag", 1.0f); set ("hall", 0.0f); set ("groesse", 1.0f); set ("schwebung", 0.0f); set ("material", 0.0f);
        juce::AudioBuffer<float> silence (2, (int) (sr * 6)); silence.clear();
        const int before = proc.ringStrikes.load();
        auto out = tt::run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m)
        {
            if (pos == 0) for (int nn : { 60, 64, 67, 72 }) m.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0);
            if (pos <= (int) (sr * 5) && pos + 512 > (int) (sr * 5)) for (int nn : { 60, 64, 67, 72 }) m.addEvent (juce::MidiMessage::noteOff (1, nn), (int) (sr * 5) - pos);
        });
        const int strikes = proc.ringStrikes.load() - before;
        float loud = 0.0f; for (int k = 0; k < 48; ++k) loud = juce::jmax (loud, out.getRMSLevel (0, k * 4800, 4800));
        // 120 BPM, Achtel = 4 Schläge/s; nach je zwei Reihen eine Pause -> in 4.9 s etwa 17 Schläge
        tt::check (tt::finite (out) && strikes >= 15 && strikes <= 20 && loud > 0.01f, juce::String::formatted ("Gelaeut: %d Anschlaege in 5 s, Pegel %.3f", strikes, loud));
        tt::writeWav (dir.getChildFile ("sanktwolfgang_gelaeut.wav"), out, sr);
        set ("gelaeut", 0.0f);
    }
    tt::synthSuite<WolfgangEditor> (proc, dir, "sanktwolfgang", "sanktwolfgang", { 0, 1, 2, 5, 6, 8 }, { 48, 55, 60, 64 }, 4.0, 8.0);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

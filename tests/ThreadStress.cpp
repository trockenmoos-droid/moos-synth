// Wie in einer DAW: Audio läuft in einem eigenen Thread, die Oberfläche wechselt gleichzeitig Presets.
#include "TestUtil.h"
#include STRESS_HEADER

// Generative Plugins (Sequenzer) spielen von selbst weiter: für die Stille-Prüfung anhalten
static void stopGenerators (juce::AudioProcessor& p)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() == "frei" || r->getParameterID() == "klatsch")
                r->setValueNotifyingHost (0.0f);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

struct AudioLoop : juce::Thread
{
    AudioLoop (juce::AudioProcessor& p, int bs) : juce::Thread ("audio"), proc (p), block (bs) {}
    juce::AudioProcessor& proc;
    int block;
    std::atomic<int> badBlocks { 0 }, loudBlocks { 0 }, blocks { 0 };
    std::atomic<float> maxPeak { 0.0f };
    std::atomic<bool> silencePhase { false };
    std::atomic<float> silencePeak { 0.0f };
    double phase = 0.0;
    void run() override
    {
        juce::AudioBuffer<float> buf (2, 2048);
        juce::Random rng (5);
        while (! threadShouldExit())
        {
            // Blockgröße schwankt wie in manchen DAWs (auch größer als vorbereitet)
            const int n = rng.nextBool() ? block : 64 + rng.nextInt (1984);
            juce::AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, n);
            // Effekte bekommen Signal (gezupfte Töne mit Rauschen), Synths Stille
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    phase += 220.0 / 48000.0; if (phase >= 1.0) phase -= 1.0;
                    const float env = silencePhase ? 0.0f : (float) std::exp (-((blocks.load() * 7 + i / 64) % 200) * 0.05);
                    b.setSample (ch, i, proc.getTotalNumInputChannels() > 0 ? env * (0.3f * (float) std::sin (phase * 6.2831853) + 0.05f * (rng.nextFloat() - 0.5f)) : 0.0f);
                }
            juce::MidiBuffer midi;
            const int k = blocks.load();
            ++blocks;
            if (! silencePhase)
            {
                if (k % 30 == 0) for (int i = 0; i < 5; ++i) midi.addEvent (juce::MidiMessage::noteOn (1, 40 + rng.nextInt (40), 0.8f), 0);
                if (k % 30 == 20) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            }
            else if (k % 30 == 0) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            proc.processBlock (b, midi);
            float pk = 0.0f; bool bad = false;
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) { const float v = b.getSample (ch, i); if (! std::isfinite (v)) bad = true; else pk = juce::jmax (pk, std::abs (v)); }
            if (bad) ++badBlocks;
            if (pk > 0.98f) ++loudBlocks;
            maxPeak = juce::jmax (maxPeak.load(), pk);
            if (silencePhase) silencePeak = pk;
            juce::Thread::sleep (1);
        }
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const int seconds = argc > 1 ? juce::String (argv[1]).getIntValue() : 30;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    const double sr = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 48000.0;
    proc->setPlayConfigDetails (proc->getTotalNumInputChannels() > 0 ? 2 : 0, 2, sr, 256);
    proc->prepareToPlay (sr, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());   // Editor offen, wie beim Benutzer

    AudioLoop audio (*proc, 256);
    audio.startThread (juce::Thread::Priority::highest);
    juce::Random rng (11);
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) seconds * 1000;
    int switches = 0;
    while (juce::Time::getMillisecondCounter() < end)
    {
        proc->setCurrentProgram (rng.nextInt (proc->getNumPrograms()));     // wie Klick auf "<" / ">"
        ++switches;
        if (switches % 3 == 0)                                               // dazu Regler bewegen
            for (auto* prm : proc->getParameters())
                if (rng.nextFloat() < 0.2f) prm->setValueNotifyingHost (rng.nextFloat());
        juce::Thread::sleep (rng.nextInt (40));
    }
    // Stille-Phase: Hall/Delay mit Feedback klingt lange aus -> ein ruhiges Preset wählen und Feedback-Regler zurück
    proc->setCurrentProgram (0);
    stopGenerators (*proc);
    audio.silencePhase = true;
    juce::Thread::sleep (25000);
    audio.stopThread (2000);
    tt::check (audio.badBlocks == 0 && audio.silencePeak.load() < 0.01f,
               juce::String (JucePlugin_Name) + juce::String::formatted (": %d Preset-Wechsel, %d Bloecke, ungueltig=%d, Vollaussteuerung=%d, Spitze=%.3f, danach Stille=%.4f",
                   switches, audio.blocks.load(), audio.badBlocks.load(), audio.loudBlocks.load(), audio.maxPeak.load(), audio.silencePeak.load()));
    ed.reset();
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

// Preset-Wechsel und zufällige Reglersprünge während gespielt wird – sucht nach NaN, Inf und Aufschaukeln
#include "TestUtil.h"
#include STRESS_HEADER

// Generative Plugins (Sequenzer) spielen von selbst weiter: für die Stille-Prüfung anhalten
static void stopGenerators (juce::AudioProcessor& p)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() == "frei" || r->getParameterID() == "klatsch" || r->getParameterID() == "freeze"
                || r->getParameterID() == "absperren" || r->getParameterID() == "kruste")
                r->setValueNotifyingHost (0.0f);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

template <typename Proc>
static void stress (const char* name, int seconds, bool randomParams, int seed)
{
    const double sr = 48000.0;
    const int block = 512;
    Proc holder;
    auto& proc = *holder.p;
    const bool effect = proc.getBusCount (true) > 0;
    proc.setPlayConfigDetails (effect ? 2 : 0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::Random rng (seed);
    const int blocks = (int) (seconds * sr / block);
    float maxPeak = 0.0f, maxLateQuiet = 0.0f;
    int firstBad = -1, loudSilentBlocks = 0;
    bool notesOn = false;

    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        // Effekte bekommen Signal: Rauschstöße und Töne, manchmal laut
        if (effect)
        {
            buf.clear();
            if ((b / 20) % 3 != 2)
            {
                const float amp = rng.nextFloat() < 0.1f ? 1.0f : 0.3f;
                const double hz = 80.0 + rng.nextInt (900);
                for (int i = 0; i < block; ++i)
                {
                    const float v = amp * (0.6f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (b * block + i) / sr)
                                           + 0.4f * (rng.nextFloat() * 2.0f - 1.0f));
                    buf.setSample (0, i, v); buf.setSample (1, i, v * 0.9f);
                }
            }
        }
        // Akkorde an/aus, Pitchbend, Mod Wheel, Aftertouch
        if (b % 40 == 0)
        {
            for (int k = 0; k < 6; ++k) midi.addEvent (juce::MidiMessage::noteOn (1, 36 + rng.nextInt (48), 0.3f + rng.nextFloat() * 0.7f), rng.nextInt (block));
            notesOn = true;
        }
        if (b % 40 == 30) { midi.addEvent (juce::MidiMessage::allNotesOff (1), 0); notesOn = false; }
        if (b % 7 == 0) midi.addEvent (juce::MidiMessage::pitchWheel (1, rng.nextInt (16384)), rng.nextInt (block));
        if (b % 5 == 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, rng.nextInt (128)), rng.nextInt (block));
        if (b % 9 == 0) midi.addEvent (juce::MidiMessage::channelPressureChange (1, rng.nextInt (128)), rng.nextInt (block));
        if (b % 13 == 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, rng.nextBool() ? 127 : 0), rng.nextInt (block));

        // Preset wechseln (wie im Editor) bzw. zufällige Sprünge
        if (b % 23 == 0) proc.setCurrentProgram (rng.nextInt (proc.getNumPrograms()));
        if (randomParams && b % 3 == 0)
            for (auto* p : proc.getParameters())
                if (rng.nextFloat() < 0.15f) p->setValueNotifyingHost (rng.nextFloat());

        proc.processBlock (buf, midi);

        float pk = 0.0f;
        bool bad = false;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (ch, i);
                if (! std::isfinite (v)) bad = true;
                else pk = juce::jmax (pk, std::abs (v));
            }
        if (bad && firstBad < 0) firstBad = b;
        maxPeak = juce::jmax (maxPeak, pk);
        juce::ignoreUnused (notesOn);
    }

    stopGenerators (proc);
    // danach: alles aus, 20 s Stille – muss leise werden
    {
        buf.clear();
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        proc.processBlock (buf, m);
    }
    for (int b = 0; b < (int) (20 * sr / block); ++b)
    {
        juce::MidiBuffer m;
        buf.clear();
        proc.processBlock (buf, m);
        float pk = 0.0f; bool bad = false;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < block; ++i) { const float v = buf.getSample (ch, i); if (! std::isfinite (v)) bad = true; else pk = juce::jmax (pk, std::abs (v)); }
        if (bad && firstBad < 0) firstBad = 100000 + b;
        if (b > (int) (17 * sr / block)) { maxLateQuiet = juce::jmax (maxLateQuiet, pk); if (pk > 0.05f) ++loudSilentBlocks; }
    }

    tt::check (firstBad < 0 && maxLateQuiet < 0.01f,
               juce::String (name) + juce::String::formatted (" (Seed %d, %s): ungueltig ab Block %d, Spitze %.3f, nach 17 s Stille noch %.4f",
                                                             seed, randomParams ? "Presets + Zufallsregler" : "nur Presets", firstBad, maxPeak, maxLateQuiet));
}

// der Prozessor kommt über die Fabrikfunktion des jeweiligen Plugins
struct AnyProc
{
    std::unique_ptr<juce::AudioProcessor> p { createPluginFilter() };
    juce::AudioProcessor* operator->() { return p.get(); }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const int seeds = argc > 1 ? juce::String (argv[1]).getIntValue() : 4;
    for (int seed = 1; seed <= seeds; ++seed)
    {
        stress<AnyProc> (JucePlugin_Name, 60, false, seed);
        stress<AnyProc> (JucePlugin_Name, 60, true,  seed);
    }
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

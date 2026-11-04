#include "TestUtil.h"
#include <thread>
#include "../plugins/hammerschmied/Source/PluginProcessor.h"
#include "../plugins/hammerschmied/Source/PluginEditor.h"

static juce::AudioBuffer<float> zeros (int n) { juce::AudioBuffer<float> b (2, n); b.clear(); return b; }

// simulierter DAW-Transport: 120 BPM; spielt, wenn playing gesetzt ist
struct TestHead : juce::AudioPlayHead
{
    bool playing = true; double ppq = 0.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (120.0); p.setIsPlaying (playing); p.setPpqPosition (ppq);
        return p;
    }
};

// Transport läuft im Takt der Blöcke mit (pos = Blockstart in Samples)
static std::function<void (int, juce::MidiBuffer&)> transport (TestHead& h, double sr, std::function<bool (double sec)> playAt = {},
                                                                 std::function<double (double sec)> ppqAt = {})
{
    return [&h, sr, playAt, ppqAt] (int pos, juce::MidiBuffer&)
    {
        const double sec = pos / sr;
        h.playing = playAt ? playAt (sec) : true;
        h.ppq = ppqAt ? ppqAt (sec) : sec * 2.0;
    };
}

static int zeroCrossings (const juce::AudioBuffer<float>& b, int start, int num)
{
    int z = 0;
    for (int i = start + 1; i < start + num; ++i) if ((b.getSample (0, i - 1) < 0.0f) != (b.getSample (0, i) < 0.0f)) ++z;
    return z;
}

static void setP (juce::AudioProcessor& p, const juce::String& id, float v)
{
    auto* a = dynamic_cast<HammerProcessor&> (p).apvts.getParameter (id);
    a->setValueNotifyingHost (a->convertTo0to1 (v));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    HammerProcessor proc;

    // Euklid: E(3,8) = x..x..x.
    {
        juce::String s; for (int i = 0; i < 8; ++i) s << (hammer::euclid (i, 8, 3, 0) ? "x" : ".");
        int n5 = 0; for (int i = 0; i < 13; ++i) n5 += hammer::euclid (i, 13, 5, 3) ? 1 : 0;
        tt::check (s == "x..x..x." && n5 == 5, "Euklid E(3,8) = " + s + ", E(5,13) hat " + juce::String (n5) + " Schläge");
    }

    juce::AudioBuffer<float> silence (2, (int) (sr * 8));
    silence.clear();
    TestHead head;
    proc.setPlayHead (&head);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto out = tt::run (proc, silence, sr, transport (head, sr));
        const float pk = tt::peak (out, 0, out.getNumSamples());
        const float rms = out.getRMSLevel (0, 0, out.getNumSamples());
        tt::check (tt::finite (out) && pk <= 1.0f && pk > 0.1f && rms > 0.01f,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Spitze=%.3f  RMS=%.3f", pk, rms));
    }

    // Host-Sync: nur der Hammer auf Schritt 1 eines 16er-Takts (alle 2 s bei 120 BPM).
    // Transport: 0-3 s spielt, 3-6 s Stopp, ab 6 s Neustart bei Takt 1
    proc.setCurrentProgram (0);
    for (int t = 0; t < 6; ++t) setP (proc, hammer::pid (t, "hit"), t == 0 ? 1.0f : 0.0f);
    for (auto id : { "chaos", "mutation", "glut", "swing", "hitze" }) setP (proc, id, 0.0f);
    setP (proc, hammer::pid (0, "chance"), 1.0f); setP (proc, hammer::pid (0, "dreh"), 0.0f);
    {
        auto out = tt::run (proc, zeros ((int) (sr * 9)), sr,
                            transport (head, sr, [] (double t) { return t < 3.0 || t >= 6.0; },
                                       [] (double t) { return t < 3.0 ? t * 2.0 : (t < 6.0 ? 6.0 : (t - 6.0) * 2.0); }));
        const float a = tt::peak (out, 0, (int) (0.05 * sr)), b = tt::peak (out, (int) (2.0 * sr), (int) (0.05 * sr));
        const float pre = tt::peak (out, (int) (1.5 * sr), (int) (0.45 * sr));
        const float stopped = tt::peak (out, (int) (3.9 * sr), (int) (2.0 * sr));
        const float restart = tt::peak (out, (int) (6.0 * sr), (int) (0.05 * sr));
        tt::check (a > 0.1f && b > 0.1f && pre < 0.02f && stopped < 1.0e-3f && restart > 0.1f,
                   juce::String::formatted ("Host-Sync: Takt 1=%.2f  Takt 2=%.2f  davor=%.3f  gestoppt=%.4f  Neustart=%.2f", a, b, pre, stopped, restart));
    }
    proc.setPlayHead (nullptr);

    // Ohne Frei-Lauf und ohne Transport: still – MIDI-Noten 36..41 schlagen die Spuren an
    proc.setCurrentProgram (0);
    setP (proc, "frei", 0.0f); setP (proc, "glut", 0.0f);
    {
        auto out = tt::run (proc, zeros ((int) sr), sr);
        tt::check (tt::peak (out, 0, out.getNumSamples()) < 1.0e-4f, juce::String::formatted ("Ohne Transport und ohne Frei: Stille (Spitze %.5f bei %d)", tt::peak (out, 0, out.getNumSamples()), [&] { int k = 0; for (int i = 0; i < out.getNumSamples(); ++i) if (std::abs (out.getSample (0, i)) > 1.0e-4f) { k = i; break; } return k; }()));
        juce::AudioBuffer<float> in (2, (int) (sr * 3)); in.clear();
        auto hit = tt::run (proc, in, sr, [&] (int pos, juce::MidiBuffer& m)
        {
            for (int t = 0; t < 6; ++t)
            {
                const int at = (int) (t * 0.4 * sr);
                if (at >= pos && at < pos + 512) m.addEvent (juce::MidiMessage::noteOn (10, 36 + t, (juce::uint8) 110), at - pos);
            }
        });
        juce::String peaks; bool ok = true;
        for (int t = 0; t < 6; ++t)
        {
            const float p = tt::peak (hit, (int) (t * 0.4 * sr), (int) (0.1 * sr));
            peaks << juce::String::formatted ("%.2f ", p);
            ok = ok && p > 0.03f;
        }
        tt::check (ok, "MIDI 36-41 spielt alle sechs Spuren: " + peaks);
    }

    // Glut verändert den Klang (Granular + Stotter mischt sich hinein)
    {
        proc.setCurrentProgram (5);  // Glitch-Geselle
        proc.setPlayHead (&head);
        setP (proc, "glut", 0.0f);
        auto dry = tt::run (proc, silence, sr, transport (head, sr));
        proc.setCurrentProgram (5);
        auto wet = tt::run (proc, silence, sr, transport (head, sr));
        double diff = 0.0;
        for (int i = 0; i < dry.getNumSamples(); ++i) diff += std::abs (dry.getSample (0, i) - wet.getSample (0, i));
        tt::check (diff / dry.getNumSamples() > 0.005, juce::String::formatted ("Glut wirkt: mittlere Abweichung %.4f", diff / dry.getNumSamples()));
    }

    // Samples: 1-kHz-Ton als Hammer-Sample; MIDI 36 spielt ihn statt des Synth-Hammers
    {
        auto wav = dir.getChildFile ("test_sample.wav");
        juce::AudioBuffer<float> sb (2, (int) (0.3 * 44100)); sb.clear();
        for (int i = 0; i < sb.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c) sb.setSample (c, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / 44100.0));
        tt::writeWav (wav, sb, 44100.0);
        proc.setCurrentProgram (0);
        setP (proc, "frei", 0.0f); setP (proc, "glut", 0.0f); setP (proc, "chaos", 0.0f); setP (proc, "hitze", 0.0f);
        setP (proc, hammer::pid (0, "abk"), 1.0f);
        const auto err = proc.loadSample (0, wav);
        const int src = (int) proc.apvts.getRawParameterValue (hammer::pid (0, "quelle"))->load();
        auto noteAt0 = [] (int pos, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 127), 10); };
        auto out = tt::run (proc, zeros ((int) sr), sr, noteAt0);
        const int zc = zeroCrossings (out, (int) (0.05 * sr), (int) (0.1 * sr));          // 1 kHz -> ~200 in 0,1 s
        tt::check (err.isEmpty() && src == 1 && zc > 170 && zc < 230 && tt::peak (out, 0, (int) (0.3 * sr)) > 0.1f,
                   juce::String::formatted ("Sample spielt: Quelle=%d, Nulldurchgaenge=%d (soll ~200)", src, zc) + (err.isEmpty() ? "" : " " + err));
        // Ton +1 = eine Oktave hoeher
        setP (proc, hammer::pid (0, "ton"), 1.0f);
        out = tt::run (proc, zeros ((int) sr), sr, noteAt0);
        const int zc2 = zeroCrossings (out, (int) (0.02 * sr), (int) (0.1 * sr));
        tt::check (zc2 > 360 && zc2 < 440, juce::String::formatted ("Sample-Tonhoehe +12: Nulldurchgaenge=%d (soll ~400)", zc2));
        // Werkspreset wechseln: Sample-Quelle bleibt
        proc.setCurrentProgram (3);
        tt::check ((int) proc.apvts.getRawParameterValue (hammer::pid (0, "quelle"))->load() == 1 && proc.sampleFor (0) != nullptr,
                   "Presetwechsel behaelt Sample und Quelle");
        // Zustand speichern/laden stellt das Sample wieder her
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        HammerProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
        tt::check (other.sampleFor (0) != nullptr && other.sampleFor (0)->name == "test_sample", "Projekt laden stellt Sample wieder her");
        // Laden/Entfernen während die Maschine spielt (Audio-Thread läuft parallel)
        {
            proc.setPlayHead (nullptr);
            setP (proc, "frei", 1.0f); setP (proc, "chaos", 0.8f);
            for (int t = 0; t < 6; ++t) setP (proc, hammer::pid (t, "quelle"), 2.0f);
            proc.prepareToPlay (sr, 256);
            std::atomic<bool> stop { false }, bad { false }; std::atomic<int> blocks { 0 };
            std::thread audio ([&]
            {
                juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
                while (! stop) { proc.processBlock (b, m); if (! tt::finite (b)) bad = true; ++blocks; }
            });
            juce::Random r (5);
            for (int i = 0; i < 300; ++i)
            {
                const int t = r.nextInt (6);
                if (r.nextBool()) proc.loadSample (t, wav); else proc.clearSample (t);
                juce::Thread::sleep (r.nextInt (6));
            }
            stop = true; audio.join();
            tt::check (! bad && blocks > 100, "300x Sample laden/entfernen waehrend der Wiedergabe: " + juce::String (blocks.load()) + " Bloecke, alles gueltig");
        }
        proc.clearSample (0);
        tt::check (proc.sampleFor (0) == nullptr && (int) proc.apvts.getRawParameterValue (hammer::pid (0, "quelle"))->load() == 0, "Sample entfernen");
    }

    // Hörprobe: alle Presets je 6 Sekunden
    {
        const int seg = (int) (sr * 6);
        juce::AudioBuffer<float> demo (2, seg * proc.getNumPrograms());
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            auto out = tt::run (proc, zeros (seg), sr, transport (head, sr));
            for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, i * seg, out, ch, 0, seg);
        }
        tt::writeWav (dir.getChildFile ("hammerschmied_demo.wav"), demo, sr);
    }

    for (int t = 0; t < 6; ++t) proc.clearSample (t);
    proc.setCurrentProgram (0);
    tt::run (proc, silence, sr, transport (head, sr));
    tt::waitForTexture ("hammerschmied", tmk::tex::landscapeFor ("hammerschmied"), dir);
    tt::screenshots<HammerEditor> (proc, dir, "hammerschmied", 1);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

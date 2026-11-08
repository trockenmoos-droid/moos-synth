#include "TestUtil.h"
#include <thread>
#include "../plugins/tusche/Source/PluginProcessor.h"
#include "../plugins/tusche/Source/PluginEditor.h"
#include "TuscheAssets.h"

static juce::AudioBuffer<float> zeros (double sr, double sec) { juce::AudioBuffer<float> b (2, (int) (sr * sec)); b.clear(); return b; }

static void setP (TuscheProcessor& p, const juce::String& id, float v)
{
    auto* a = p.apvts.getParameter (id);
    a->setValueNotifyingHost (a->convertTo0to1 (v));
}

struct TestHead : juce::AudioPlayHead
{
    bool playing = false; double ppq = 0.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (120.0); p.setIsPlaying (playing); p.setPpqPosition (ppq); return p;
    }
};

static int zeroCrossings (const juce::AudioBuffer<float>& b, int start, int num)
{
    int z = 0;
    for (int i = start + 1; i < start + num; ++i) if ((b.getSample (0, i - 1) < 0.0f) != (b.getSample (0, i) < 0.0f)) ++z;
    return z;
}

// Noten: (Zeit in s, Note, an/aus)
static std::function<void (int, juce::MidiBuffer&)> notes (double sr, std::vector<std::tuple<double, int, bool>> ev, TestHead* head = nullptr,
                                                            std::function<bool (double)> play = {})
{
    return [=] (int pos, juce::MidiBuffer& m)
    {
        for (auto& [t, n, on] : ev)
        {
            const int at = (int) (t * sr);
            if (at >= pos && at < pos + 512)
                m.addEvent (on ? juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, n), at - pos);
        }
        if (head != nullptr) { head->playing = play ? play (pos / sr) : false; head->ppq = pos / sr * 2.0; }
    };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    TuscheProcessor proc;
    TestHead head;
    proc.setPlayHead (&head);

    // alle Presets: Akkord / Tasten / Schritte mit laufendem Transport
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto out = tt::run (proc, zeros (sr, 6.0), sr,
                            notes (sr, { { 0.1, 60, true }, { 0.1, 64, true }, { 0.6, 67, true }, { 2.5, 60, false }, { 2.5, 64, false }, { 2.5, 67, false } },
                                   &head, [] (double t) { return t < 3.0; }));
        const float pk = tt::peak (out, 0, out.getNumSamples());
        const float act = out.getRMSLevel (0, (int) (0.5 * sr), (int) (1.5 * sr));
        tt::check (tt::finite (out) && pk <= 1.0f && act > 0.01f,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Spitze=%.3f  RMS=%.3f", pk, act));
    }

    // Halten: klingt nach dem Loslassen weiter (Latch); neuer Akkord ersetzt
    {
        proc.setCurrentProgram (0);
        auto out = tt::run (proc, zeros (sr, 4.0), sr, notes (sr, { { 0.1, 60, true }, { 0.4, 60, false } }));
        const float held = out.getRMSLevel (0, (int) (2.0 * sr), (int) sr);
        tt::check (held > 0.01f, juce::String::formatted ("Halten: klingt nach dem Loslassen weiter (RMS %.3f)", held));
    }

    // Schritt: läuft mit dem Transport, Stopp -> still
    {
        proc.setCurrentProgram (3);   // Pinselschläge
        auto out = tt::run (proc, zeros (sr, 6.0), sr, notes (sr, {}, &head, [] (double t) { return t < 2.5; }));
        const float run = out.getRMSLevel (0, (int) (0.5 * sr), (int) (1.5 * sr));
        const float stop = out.getRMSLevel (0, (int) (4.5 * sr), (int) (1.0 * sr));
        tt::check (run > 0.01f && stop < 1.0e-3f, juce::String::formatted ("Schritt folgt dem Transport: laeuft=%.3f  gestoppt=%.5f", run, stop));
    }

    // Tasten: zwei Tasten spielen zwei verschiedene Striche
    {
        proc.setCurrentProgram (9);   // Tastenstriche
        for (int c = 0; c < tusche::numChops; ++c) setP (proc, tusche::cid (c, "chaos"), 0.0f);
        auto a = tt::run (proc, zeros (sr, 1.5), sr, notes (sr, { { 0.05, 60, true } }));
        auto b = tt::run (proc, zeros (sr, 1.5), sr, notes (sr, { { 0.05, 68, true } }));
        double diff = 0.0;
        for (int i = (int) (0.3 * sr); i < (int) (1.2 * sr); ++i) diff += std::abs (a.getSample (0, i) - b.getSample (0, i));
        tt::check (diff / (0.9 * sr) > 0.005 && a.getRMSLevel (0, 0, a.getNumSamples()) > 0.01f,
                   juce::String::formatted ("Tasten: C3 und G#3 spielen verschiedene Striche (Abweichung %.4f)", diff / (0.9 * sr)));
    }

    // Eigenes Sample: 500-Hz-Ton; C4 (Note 72) klingt eine Oktave höher als C3
    {
        auto wav = dir.getChildFile ("tusche_test.wav");
        juce::AudioBuffer<float> sb (1, 44100 * 2);
        for (int i = 0; i < sb.getNumSamples(); ++i) sb.setSample (0, i, 0.6f * (float) std::sin (juce::MathConstants<double>::twoPi * 500.0 * i / 44100.0));
        juce::AudioBuffer<float> st (2, sb.getNumSamples()); st.copyFrom (0, 0, sb, 0, 0, sb.getNumSamples()); st.copyFrom (1, 0, sb, 0, 0, sb.getNumSamples());
        tt::writeWav (wav, st, 44100.0);
        proc.setCurrentProgram (0);
        const auto err = proc.loadSample (wav);
        for (int c = 0; c < tusche::numChops; ++c) { setP (proc, tusche::cid (c, "chaos"), 0.0f); setP (proc, tusche::cid (c, "raum"), 0.0f); }
        setP (proc, "raumgr", 0.0f);
        const int q = (int) proc.apvts.getRawParameterValue ("quelle")->load();
        auto lo = tt::run (proc, zeros (sr, 1.5), sr, notes (sr, { { 0.05, 60, true } }));
        auto hi = tt::run (proc, zeros (sr, 1.5), sr, notes (sr, { { 0.05, 72, true } }));
        const int z0 = zeroCrossings (lo, (int) (0.4 * sr), (int) (0.5 * sr)), z1 = zeroCrossings (hi, (int) (0.4 * sr), (int) (0.5 * sr));
        const double ratio = (double) z1 / juce::jmax (1, z0);
        tt::check (err.isEmpty() && q == 4 && z0 > 250 && ratio > 1.9 && ratio < 2.1,
                   juce::String::formatted ("Eigenes Sample: Quelle=%d, C4/C3 Nulldurchgaenge %d/%d = %.2f (Oktave = 2)", q, z1, z0, ratio) + err);
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        TuscheProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
        tt::check (other.currentSource() != nullptr && other.currentSource()->name == "tusche_test", "Projekt laden stellt das Sample wieder her");

        // Laden/Entfernen während der Wiedergabe
        proc.setPlayHead (nullptr);
        proc.prepareToPlay (sr, 256);
        std::atomic<bool> stop { false }, bad { false };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> b (2, 256);
            int k = 0;
            while (! stop)
            {
                juce::MidiBuffer m;
                if (k % 40 == 0) m.addEvent (juce::MidiMessage::noteOn (1, 55 + (k / 40) % 12, (juce::uint8) 100), 0);
                proc.processBlock (b, m);
                if (! tt::finite (b)) bad = true;
                ++k;
            }
        });
        juce::Random r (3);
        for (int i = 0; i < 150; ++i) { if (r.nextBool()) proc.loadSample (wav); else proc.clearSample(); juce::Thread::sleep (r.nextInt (8)); }
        stop = true; audio.join();
        tt::check (! bad, "150x Sample laden/entfernen waehrend der Wiedergabe");
        proc.clearSample();
        proc.setPlayHead (&head);
    }

    // Wie in Ableton: 44,1 kHz, wechselnde Blockgrößen, Pinselschläge spielen, dann Stopp und lange nichts
    for (int round = 0; round < 6; ++round)
    {
        const double sr2 = 44100.0;
        proc.setCurrentProgram (3);
        proc.setPlayConfigDetails (0, 2, sr2, 512);
        proc.prepareToPlay (sr2, 512);
        juce::Random r (round + 11);
        double t = 0.0; float worstAfterStop = 0.0f, worst = 0.0f; bool bad = false;
        const double playFor = 5.0 + round * 7.0;
        while (t < playFor + 60.0)
        {
            const int bs = round % 2 == 0 ? 512 : 32 + r.nextInt (1000);
            juce::AudioBuffer<float> b (2, bs); b.clear();
            juce::MidiBuffer m;
            head.playing = t < playFor; head.ppq = t * 2.0;
            proc.processBlock (b, m);
            const float pk = b.getMagnitude (0, bs);
            if (! tt::finite (b)) bad = true;
            worst = juce::jmax (worst, pk);
            if (t > playFor + 3.0) worstAfterStop = juce::jmax (worstAfterStop, pk);
            t += bs / sr2;
        }
        tt::check (! bad && worstAfterStop < 1.0e-3f,
                   juce::String::formatted ("Ableton-Szenario %d: %.0f s spielen, 60 s Stopp: Spitze=%.3f, nach Stopp=%.5f", round, playFor, worst, worstAfterStop));
    }

    // Hörprobe
    {
        const int sel[] = { 1, 3, 5, 7, 10 };
        const int seg = (int) (sr * 7.0);
        juce::AudioBuffer<float> demo (2, seg * 5);
        for (int j = 0; j < 5; ++j)
        {
            proc.setCurrentProgram (sel[j]);
            const bool keys = sel[j] >= 9;
            std::vector<std::tuple<double, int, bool>> ev;
            if (keys) { const int ns[] = { 60, 62, 64, 67, 69, 72, 64, 60 }; for (int k = 0; k < 8; ++k) { ev.push_back ({ 0.1 + k * 0.6, ns[k], true }); ev.push_back ({ 0.5 + k * 0.6, ns[k], false }); } }
            else { ev = { { 0.1, 57, true }, { 0.1, 64, true }, { 0.1, 69, true }, { 3.0, 57, false }, { 3.0, 64, false }, { 3.0, 69, false },
                          { 3.2, 53, true }, { 3.2, 60, true }, { 3.2, 65, true }, { 5.5, 53, false }, { 5.5, 60, false }, { 5.5, 65, false } }; }
            auto out = tt::run (proc, zeros (sr, 7.0), sr, notes (sr, ev, &head, [] (double t) { return t < 6.0; }));
            for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, j * seg, out, ch, 0, seg);
        }
        tt::writeWav (dir.getChildFile ("tusche_demo.wav"), demo, sr);
    }

    proc.setCurrentProgram (1);
    setP (proc, "auswahl", 5.0f);
    tt::run (proc, zeros (sr, 2.0), sr, notes (sr, { { 0.05, 60, true }, { 0.05, 67, true } }));
    tt::waitForTexture ("tusche", [] { return juce::ImageFileFormat::loadFrom (TuscheAssets::papier_jpg, (size_t) TuscheAssets::papier_jpgSize); }, dir);
    tt::screenshots<TuscheEditor> (proc, dir, "tusche", 1);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Params.h"

// Arpeggiator: wandelt gehaltene Noten in eine Notenfolge um.
// Läuft der Host, rastet er auf dessen Taktraster ein; sonst frei mit Host-Tempo (oder 120 BPM).
class Arpeggiator
{
public:
    struct Settings { bool on; int mode; int rate; int octaves; float gate; double bpm; bool hostPlaying; double ppq; };

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset()
    {
        held.clear(); step = 0; current = -1; offCountdown = -1; samplesToStep = 0.0; wasOn = false;
    }

    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, const Settings& s)
    {
        out.clear();

        if (! s.on)
        {
            if (wasOn)
            {
                if (current >= 0) out.addEvent (juce::MidiMessage::noteOff (1, current), 0);
                // gehaltene Tasten wieder normal anspielen
                for (auto& h : held) out.addEvent (juce::MidiMessage::noteOn (1, h.note, h.vel), 0);
                reset();
            }
            for (const auto m : in) out.addEvent (m.getMessage(), m.samplePosition);
            return;
        }

        if (! wasOn)
        {
            // gerade eingeschaltet: normal klingende Noten beenden
            for (auto& h : held) out.addEvent (juce::MidiMessage::noteOff (1, h.note), 0);
            wasOn = true;
        }

        const double beats     = MoosParams::arpRateBeats (s.rate);
        const double bpm       = s.bpm > 1.0 ? s.bpm : 120.0;
        const double stepLen   = beats * 60.0 / bpm * sr;
        const int    gateLen   = juce::jmax (1, (int) (stepLen * s.gate));

        // Schritt-Zeitpunkte in diesem Block bestimmen
        std::vector<int> stepTimes;
        if (s.hostPlaying)
        {
            const double samplesPerBeat = 60.0 / bpm * sr;
            double next = std::ceil (s.ppq / beats - 1.0e-9) * beats;
            for (;;)
            {
                const int t = (int) std::round ((next - s.ppq) * samplesPerBeat);
                if (t >= numSamples) break;
                if (t >= 0) stepTimes.push_back (t);
                next += beats;
            }
        }

        auto it = in.begin();
        for (int t = 0; t < numSamples; ++t)
        {
            bool startedNow = false;
            for (; it != in.end() && (*it).samplePosition <= t; ++it)
            {
                const auto m = (*it).getMessage();
                if (m.isNoteOn())
                {
                    if (held.empty()) startedNow = true;
                    held.push_back ({ m.getNoteNumber(), m.getFloatVelocity(), order++ });
                }
                else if (m.isNoteOff())
                {
                    held.erase (std::remove_if (held.begin(), held.end(),
                                [&] (const Held& h) { return h.note == m.getNoteNumber(); }), held.end());
                    if (held.empty())
                    {
                        if (current >= 0) out.addEvent (juce::MidiMessage::noteOff (1, current), t);
                        current = -1; offCountdown = -1; step = 0;
                    }
                }
                else out.addEvent (m, t);
            }

            if (offCountdown >= 0 && --offCountdown < 0 && current >= 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, current), t);
                current = -1;
            }

            bool doStep = false;
            if (s.hostPlaying)
                doStep = std::find (stepTimes.begin(), stepTimes.end(), t) != stepTimes.end();
            else
            {
                if (startedNow) samplesToStep = 0.0;
                samplesToStep -= 1.0;
                if (samplesToStep <= 0.0) { doStep = true; samplesToStep += stepLen; }
            }

            if (doStep && ! held.empty())
            {
                if (current >= 0) out.addEvent (juce::MidiMessage::noteOff (1, current), t);
                const auto [note, vel] = nextNote (s);
                out.addEvent (juce::MidiMessage::noteOn (1, note, vel), t);
                current = note;
                offCountdown = gateLen;
            }
        }
    }

private:
    struct Held { int note; float vel; juce::uint64 order; };
    std::vector<Held> held;
    juce::uint64 order = 0;
    double sr = 44100.0, samplesToStep = 0.0;
    int step = 0, current = -1, offCountdown = -1;
    bool wasOn = false;
    juce::Random rng;

    std::pair<int, float> nextNote (const Settings& s)
    {
        std::vector<Held> base = held;
        if (s.mode != 4)   // alle außer "As Played" nach Tonhöhe sortieren
            std::sort (base.begin(), base.end(), [] (auto& a, auto& b) { return a.note < b.note; });
        else
            std::sort (base.begin(), base.end(), [] (auto& a, auto& b) { return a.order < b.order; });

        std::vector<Held> seq;
        for (int o = 0; o < juce::jmax (1, s.octaves); ++o)
            for (auto h : base)
                if (h.note + 12 * o <= 127) { h.note += 12 * o; seq.push_back (h); }

        const int n = (int) seq.size();
        int idx = 0;
        switch (s.mode)
        {
            case 1: idx = n - 1 - (step % n); break;
            case 2:
            {
                const int cycle = juce::jmax (1, 2 * n - 2);
                const int p = step % cycle;
                idx = p < n ? p : cycle - p;
                break;
            }
            case 3: idx = rng.nextInt (n); break;
            default: idx = step % n; break;
        }
        ++step;
        return { seq[(size_t) juce::jlimit (0, n - 1, idx)].note, seq[(size_t) juce::jlimit (0, n - 1, idx)].vel };
    }
};

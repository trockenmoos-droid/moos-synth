#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace tmk
{
    // Gemeinsame Spielhilfen-Werte für alle Stimmen
    struct PlayContext
    {
        float bend = 0.0f;        // Halbtöne
        float modWheel = 0.0f;    // 0..1
        float aftertouch = 0.0f;  // 0..1
        float bendRange = 2.0f;
    };

    // Basis einer Stimme: Unterklassen implementieren start/release/render
    struct VoiceBase
    {
        bool active = false, held = false, sustained = false;
        int note = 60;
        float velocity = 1.0f;
        juce::uint64 age = 0;
        float polyPressure = 0.0f;
        static double hz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }
    };

    // Polyphone Stimmenverwaltung: freie Stimme > losgelassene älteste > älteste; Sustain-Pedal; MIDI sampgenau
    template <typename VoiceT, int N>
    class VoiceManager
    {
    public:
        VoiceT voices[N];
        PlayContext ctx;

        void reset()
        {
            for (auto& v : voices) { v.kill(); v.active = v.held = v.sustained = false; }
            sustain = false;
        }

        // rendert den Block, MIDI-Ereignisse an ihrer Sample-Position
        void process (float* L, float* R, int numSamples, const juce::MidiBuffer& midi)
        {
            int pos = 0;
            for (const auto m : midi)
            {
                const int t = juce::jlimit (0, numSamples, m.samplePosition);
                if (t > pos) { render (L + pos, R + pos, t - pos); pos = t; }
                handle (m.getMessage());
            }
            if (pos < numSamples) render (L + pos, R + pos, numSamples - pos);
        }

        int activeCount() const { int c = 0; for (auto& v : voices) c += v.active ? 1 : 0; return c; }

    private:
        bool sustain = false;
        juce::uint64 counter = 0;

        void render (float* L, float* R, int n)
        {
            for (auto& v : voices)
                if (v.active) v.render (L, R, n, ctx);
        }

        void handle (const juce::MidiMessage& m)
        {
            if (m.isNoteOn())
            {
                VoiceT* t = nullptr;
                for (auto& v : voices) if (v.active && v.note == m.getNoteNumber()) { t = &v; break; }
                if (t == nullptr) for (auto& v : voices) if (! v.active) { t = &v; break; }
                if (t == nullptr)
                {
                    juce::uint64 best = std::numeric_limits<juce::uint64>::max();
                    for (auto& v : voices) if (! v.held && ! v.sustained && v.age < best) { best = v.age; t = &v; }
                    if (t == nullptr) for (auto& v : voices) if (v.age < best) { best = v.age; t = &v; }
                }
                const bool wasActive = t->active;
                t->active = true; t->held = true; t->sustained = false;
                t->note = m.getNoteNumber(); t->velocity = m.getFloatVelocity(); t->age = ++counter; t->polyPressure = 0.0f;
                t->start (wasActive);
            }
            else if (m.isNoteOff())
            {
                for (auto& v : voices)
                    if (v.active && v.held && v.note == m.getNoteNumber())
                    {
                        v.held = false;
                        if (sustain) v.sustained = true; else v.release();
                    }
            }
            else if (m.isSustainPedalOn()) sustain = true;
            else if (m.isSustainPedalOff())
            {
                sustain = false;
                for (auto& v : voices) if (v.sustained && ! v.held) { v.sustained = false; v.release(); }
            }
            else if (m.isPitchWheel())      ctx.bend = (float) ((m.getPitchWheelValue() - 8192) / 8192.0) * ctx.bendRange;
            else if (m.isController() && m.getControllerNumber() == 1) ctx.modWheel = m.getControllerValue() / 127.0f;
            else if (m.isChannelPressure()) ctx.aftertouch = m.getChannelPressureValue() / 127.0f;
            else if (m.isAftertouch())
            {
                for (auto& v : voices) if (v.active && v.note == m.getNoteNumber()) v.polyPressure = m.getAfterTouchValue() / 127.0f;
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                for (auto& v : voices) { v.held = v.sustained = false; v.release(); }
        }
    };

    // Kleine DSP-Bausteine
    namespace dsp
    {
        // TPT-Zustandsvariablen-Filter
        struct Svf
        {
            float ic1 = 0.0f, ic2 = 0.0f, g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
            void set (double fc, double sr, float q)
            {
                g = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, 0.45 * sr, fc) / sr);
                k = 1.0f / juce::jmax (0.05f, q);
                a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
            }
            // liefert Tiefpass, Bandpass (normiert: Spitze 1), Hochpass
            inline void process (float x, float& lp, float& bp, float& hp) noexcept
            {
                const float v3 = x - ic2;
                const float v1 = a1 * ic1 + a2 * v3;
                const float v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
                lp = v2; bp = k * v1; hp = x - k * v1 - v2;
            }
            inline float bandpass (float x) noexcept { float l, b, h; process (x, l, b, h); return b; }
            inline float lowpass (float x) noexcept  { float l, b, h; process (x, l, b, h); return l; }
            void reset() { ic1 = ic2 = 0.0f; }
        };

        // Gedämpfter Resonator (eine Mode): Impuls -> abklingender Sinus
        struct Mode
        {
            float y1 = 0, y2 = 0, b1 = 0, b2 = 0, gain = 0;
            void set (double freq, double decaySec, double sr, float amp)
            {
                const double r = std::exp (-1.0 / (juce::jmax (0.001, decaySec) * sr));
                b1 = (float) (2.0 * r * std::cos (juce::MathConstants<double>::twoPi * juce::jmin (freq, 0.47 * sr) / sr));
                b2 = (float) (-r * r);
                gain = amp;
            }
            inline float process (float x) noexcept
            {
                const float y = b1 * y1 + b2 * y2 + x;
                y2 = y1; y1 = y;
                return y * gain;
            }
            void reset() { y1 = y2 = 0.0f; }
        };

        inline float onePoleCoef (double fc, double sr) { return (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jlimit (1.0, 0.49 * sr, fc) / sr); }

        // Sinus-Tabelle (geteilt)
        inline const std::vector<float>& sineTable()
        {
            static const std::vector<float> t = []
            {
                std::vector<float> v (4097);
                for (int i = 0; i <= 4096; ++i) v[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / 4096.0);
                return v;
            }();
            return t;
        }
        inline float sine (double phase) noexcept      // phase 0..1
        {
            const auto& t = sineTable();
            const double idx = phase * 4096.0;
            const int i = (int) idx;
            const float f = (float) (idx - i);
            return t[(size_t) i] + f * (t[(size_t) i + 1] - t[(size_t) i]);
        }

        // PolyBLEP-Sägezahn (bandbegrenzt)
        inline float polyBlep (double t, double dt) noexcept
        {
            if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
            return 0.0f;
        }
        inline float sawBlep (double phase, double inc) noexcept { return (float) (2.0 * phase - 1.0) - polyBlep (phase, inc); }
    }
}

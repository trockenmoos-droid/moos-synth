#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Wavetable.h"

struct PadSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Rohzeiger auf alle Parameter (vom AudioProcessorValueTreeState)
struct PadParams
{
    std::atomic<float>* posA {}; std::atomic<float>* octA {}; std::atomic<float>* levelA {};
    std::atomic<float>* posB {}; std::atomic<float>* octB {}; std::atomic<float>* fineB {}; std::atomic<float>* levelB {};
    std::atomic<float>* uniVoices {}; std::atomic<float>* uniDetune {}; std::atomic<float>* uniWidth {};
    std::atomic<float>* cutoff {}; std::atomic<float>* reso {}; std::atomic<float>* envAmt {};
    std::atomic<float>* fA {}; std::atomic<float>* fD {}; std::atomic<float>* fS {}; std::atomic<float>* fR {};
    std::atomic<float>* aA {}; std::atomic<float>* aD {}; std::atomic<float>* aS {}; std::atomic<float>* aR {};
    std::atomic<float>* lfoRate {}; std::atomic<float>* lfoPos {}; std::atomic<float>* lfoCut {};
};

class PadVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int maxUnison = 7;

    PadVoice (const WavetableBank& b, const PadParams& p) : bank (b), params (p) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PadSound*> (s) != nullptr; }

    void startNote (int midiNote, float vel, juce::SynthesiserSound*, int pitchWheel) override
    {
        noteHz   = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        velocity = vel;
        pitchWheelMoved (pitchWheel);

        for (auto& osc : phases)
            for (auto& ph : osc)
                ph = rng.nextDouble();               // zufällige Startphasen = breiter, lebendiger Pad-Sound
        lfoPhase = rng.nextDouble();

        ic1L = ic2L = ic1R = ic2R = 0.0f;
        updateEnvelopes();
        ampEnv.noteOn();
        fltEnv.noteOn();
        lastPosA = params.posA->load();
        lastPosB = params.posB->load();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnv.noteOff();
            fltEnv.noteOff();
        }
        else
        {
            ampEnv.reset();
            fltEnv.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int value) override
    {
        bend = std::pow (2.0, ((value - 8192) / 8192.0) * 2.0 / 12.0);   // ±2 Halbtöne
    }

    void controllerMoved (int, int) override {}
    using juce::SynthesiserVoice::renderNextBlock;

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive())
            return;

        const double sr = getSampleRate();
        updateEnvelopes();

        // --- Oszillator-Frequenzen für diesen Block ---
        const int    nUni    = juce::jlimit (1, maxUnison, (int) std::round (params.uniVoices->load()));
        const float  spread  = params.uniDetune->load();     // Cent
        const float  width   = params.uniWidth->load();
        const float  uniGain = 1.0f / std::sqrt ((float) nUni);
        const float  lvl[2]  = { params.levelA->load(), params.levelB->load() };
        const double oscHz[2] = {
            noteHz * bend * std::pow (2.0, (double) std::round (params.octA->load())),
            noteHz * bend * std::pow (2.0, (double) std::round (params.octB->load()) + params.fineB->load() / 1200.0)
        };

        double inc[2][maxUnison];
        float  panL[maxUnison], panR[maxUnison];
        int    level[2];

        for (int u = 0; u < nUni; ++u)
        {
            const float t = (nUni == 1) ? 0.0f : (2.0f * (float) u / (float) (nUni - 1) - 1.0f);
            const double detune = std::pow (2.0, (t * spread) / 1200.0);
            for (int o = 0; o < 2; ++o)
                inc[o][u] = oscHz[o] * detune / sr;

            const float angle = (t * width + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            panL[u] = std::cos (angle) * uniGain;
            panR[u] = std::sin (angle) * uniGain;
        }
        for (int o = 0; o < 2; ++o)
            level[o] = WavetableBank::levelForFrequency (oscHz[o] * std::pow (2.0, spread / 1200.0), sr);

        // --- Filter / LFO ---
        const float baseCut = params.cutoff->load();
        const float k       = 2.0f - 1.96f * params.reso->load();
        const float envAmt  = params.envAmt->load();
        const float lfoCut  = params.lfoCut->load();
        const float lfoPosA = params.lfoPos->load();
        const double lfoInc = params.lfoRate->load() / sr;
        const float ampVel  = 0.3f + 0.7f * velocity;

        auto* L = out.getWritePointer (0);
        auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

        constexpr int sub = 16;
        for (int s0 = 0; s0 < num; s0 += sub)
        {
            const int n = juce::jmin (sub, num - s0);

            const float lfo  = (float) std::sin (lfoPhase * juce::MathConstants<double>::twoPi);
            lfoPhase += lfoInc * n;
            lfoPhase -= std::floor (lfoPhase);

            const float targetPos[2] = {
                juce::jlimit (0.0f, 1.0f, params.posA->load() + 0.5f * lfoPosA * lfo),
                juce::jlimit (0.0f, 1.0f, params.posB->load() + 0.5f * lfoPosA * lfo)
            };

            const float fenv = fltEnv.getNextSample();
            for (int i = 1; i < n; ++i) fltEnv.getNextSample();

            const float cutHz = juce::jlimit (20.0f, (float) (0.45 * sr),
                                              baseCut * std::pow (2.0f, envAmt * 6.0f * fenv + lfoCut * 3.0f * lfo));
            const float g  = (float) std::tan (juce::MathConstants<double>::pi * cutHz / sr);
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;

            float posStart[2] = { lastPosA, lastPosB };

            for (int i = 0; i < n; ++i)
            {
                float sl = 0.0f, sr_ = 0.0f;
                const float frac = (float) (i + 1) / (float) n;

                for (int o = 0; o < 2; ++o)
                {
                    if (lvl[o] <= 0.0001f) continue;

                    const float pos = posStart[o] + (targetPos[o] - posStart[o]) * frac;
                    const float fp  = pos * (WavetableBank::numFrames - 1);
                    const int   f0  = juce::jmin ((int) fp, WavetableBank::numFrames - 1);
                    const int   f1  = juce::jmin (f0 + 1, WavetableBank::numFrames - 1);
                    const float ff  = fp - (float) f0;
                    const float* t0 = bank.get (level[o], f0);
                    const float* t1 = bank.get (level[o], f1);

                    for (int u = 0; u < nUni; ++u)
                    {
                        double& ph = phases[o][u];
                        const double idx = ph * WavetableBank::tableSize;
                        const int    ii  = (int) idx;
                        const float  fr  = (float) (idx - ii);
                        const float  a   = t0[ii] + fr * (t0[ii + 1] - t0[ii]);
                        const float  b   = t1[ii] + fr * (t1[ii + 1] - t1[ii]);
                        const float  v   = (a + ff * (b - a)) * lvl[o];

                        sl  += v * panL[u];
                        sr_ += v * panR[u];

                        ph += inc[o][u];
                        if (ph >= 1.0) ph -= 1.0;
                    }
                }

                // Zustandsvariablen-Filter (TPT/Zavalishin), Tiefpass, stereo
                const float outL = svf (sl,  ic1L, ic2L, a1, a2, a3);
                const float outR = svf (sr_, ic1R, ic2R, a1, a2, a3);

                const float env = ampEnv.getNextSample() * ampVel * 0.35f;
                L[start + s0 + i] += outL * env;
                if (R != nullptr) R[start + s0 + i] += outR * env;
            }

            lastPosA = targetPos[0];
            lastPosB = targetPos[1];

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    const WavetableBank& bank;
    const PadParams& params;

    juce::ADSR ampEnv, fltEnv;
    juce::Random rng;

    double phases[2][maxUnison] {};
    double noteHz = 440.0, bend = 1.0, lfoPhase = 0.0;
    float velocity = 1.0f, lastPosA = 0.0f, lastPosB = 0.0f;
    float ic1L = 0, ic2L = 0, ic1R = 0, ic2R = 0;

    static inline float svf (float x, float& ic1, float& ic2, float a1, float a2, float a3) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    void updateEnvelopes()
    {
        ampEnv.setSampleRate (getSampleRate());
        fltEnv.setSampleRate (getSampleRate());
        ampEnv.setParameters ({ params.aA->load(), params.aD->load(), params.aS->load(), params.aR->load() });
        fltEnv.setParameters ({ params.fA->load(), params.fD->load(), params.fS->load(), params.fR->load() });
    }
};

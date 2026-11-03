#pragma once

#include "TmVoices.h"

namespace tmk::inst
{
    // Blechblas-Kern (Carnyx, Cornu): obertonreicher Sägezahn, Helligkeit folgt der Lautstärke
    // (typisch für Blechbläser), Growl (rauhe Lippen), Zunge (Rasseln), Schalltrichter-Formant.
    struct Horn
    {
        double phase = 0.0;
        tmk::dsp::Svf lp, formant;
        float growlLp = 0.0f, swoop = 0.0f;
        double rattlePhase = 0.0;
        int counter = 0;

        struct Settings { float hell = 0.5f, growl = 0.0f, zunge = 0.0f, formantHz = 700.0f, formantAmt = 0.3f; };

        void start (float swoopSemis, bool keepPhase, juce::Random& rng)
        {
            swoop = swoopSemis;
            if (! keepPhase) phase = rng.nextDouble();
            lp.reset(); formant.reset();
        }

        inline float process (double freq, float env, const Settings& s, double sr, juce::Random& rng) noexcept
        {
            swoop *= (float) (1.0 - 1.0 / (0.06 * sr));                        // Anblasen: von unten in den Ton (~60 ms)
            const float noise = rng.nextFloat() * 2.0f - 1.0f;
            growlLp += 0.02f * (noise - growlLp);                              // langsames, rauhes Rauschen
            const double f = freq * std::pow (2.0, (swoop + s.growl * 0.25f * growlLp) / 12.0);
            const double inc = f / sr;
            phase += inc; if (phase >= 1.0) phase -= 1.0;
            float x = tmk::dsp::sawBlep (phase, inc);

            if ((counter++ & 15) == 0)
            {
                const float e = juce::jlimit (0.0f, 1.0f, env);
                lp.set (juce::jmin (0.45 * sr, f * (1.2 + 16.0 * s.hell * std::pow (e, 1.5f))), sr, 0.9f);
                formant.set (s.formantHz, sr, 3.0f);
            }
            float y = lp.lowpass (x);
            y += formant.bandpass (y) * s.formantAmt * 2.0f;
            // Growl als Amplitudenrauheit, Zunge als Rasseln (~28 Hz)
            y *= 1.0f + s.growl * 0.6f * growlLp * 4.0f;
            if (s.zunge > 0.0f)
            {
                rattlePhase += 28.0 / sr; if (rattlePhase >= 1.0) rattlePhase -= 1.0;
                y *= 1.0f - s.zunge * 0.55f * (rattlePhase < 0.5 ? 1.0f : 0.0f);
            }
            return y;
        }
    };

    // Lippenpfeife (Hydraulis, Bordun): Grundton + ungerade Obertöne + Anblasrauschen
    struct Pipe
    {
        double phase = 0.0;
        tmk::dsp::Svf breath;
        int counter = 0;
        void start (juce::Random& rng) { phase = rng.nextDouble(); breath.reset(); }
        inline float process (double freq, float luft, double sr, juce::Random& rng) noexcept
        {
            phase += freq / sr; if (phase >= 1.0) phase -= 1.0;
            float y = tmk::dsp::sine (phase) + 0.18f * tmk::dsp::sine (std::fmod (phase * 3.0, 1.0)) + 0.07f * tmk::dsp::sine (std::fmod (phase * 5.0, 1.0));
            if ((counter++ & 31) == 0) breath.set (juce::jmin (0.45 * sr, freq * 2.0), sr, 6.0f);
            y += breath.bandpass (rng.nextFloat() * 2.0f - 1.0f) * luft * 1.5f;
            return y;
        }
    };
}

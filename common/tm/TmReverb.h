#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace tmk
{
    // Später Nachhall: 8-Kanal-Feedback-Delay-Netzwerk (Householder-Matrix),
    // frequenzabhängige Nachhallzeit (Bass / Mitten / Höhen), Modulation, Eingangs-Diffusion, Freeze.
    class FdnReverb
    {
    public:
        static constexpr int N = 8;

        struct Settings
        {
            float sizeScale = 1.0f;     // Multiplikator der Grund-Laufzeiten (0.3 .. 3)
            float decay     = 3.0f;     // RT60 in Sekunden (Mitten)
            float bassRatio = 1.0f;     // RT60-Faktor unterhalb ~250 Hz
            float highRatio = 0.5f;     // RT60-Faktor oberhalb highCut
            float highCut   = 5000.0f;  // Übergang Mitten/Höhen
            float modDepth  = 0.3f;     // 0..1
            float modRate   = 0.6f;     // Hz
            float diffusion = 0.7f;     // 0..1
            float width     = 1.0f;     // 0..1
            bool  freeze    = false;
        };

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            const int maxLen = (int) (sr * 0.65) + 64;
            for (auto& l : lines) { l.buf.assign ((size_t) maxLen, 0.0f); l.w = 0; l.lpLow = l.lpHigh = 0.0f; }
            for (int c = 0; c < 2; ++c)
                for (int a = 0; a < 4; ++a)
                {
                    auto& ap = allpass[c][a];
                    ap.buf.assign ((size_t) ((apMs[a] * (c == 0 ? 1.0f : 1.07f)) * 0.001 * sr) + 2, 0.0f);
                    ap.idx = 0;
                }
            juce::Random r (7);
            for (int i = 0; i < N; ++i) lfoPhase[i] = r.nextFloat();
            update (current);
        }

        void reset()
        {
            for (auto& l : lines) { std::fill (l.buf.begin(), l.buf.end(), 0.0f); l.lpLow = l.lpHigh = 0.0f; }
            for (auto& c : allpass) for (auto& a : c) std::fill (a.buf.begin(), a.buf.end(), 0.0f);
        }

        void update (const Settings& s)
        {
            current = s;
            const float scale = juce::jlimit (0.3f, 3.0f, s.sizeScale);
            const float rt    = juce::jmax (0.05f, s.decay);
            for (int i = 0; i < N; ++i)
            {
                auto& l = lines[i];
                l.length = juce::jlimit (16.0f, (float) l.buf.size() - 40.0f, baseMs[i] * scale * 0.001f * (float) sr);
                auto gainFor = [&] (float t) { return (float) std::pow (10.0, -3.0 * l.length / (juce::jmax (0.05f, t) * sr)); };
                l.gMid  = s.freeze ? 1.0f : juce::jmin (0.99995f, gainFor (rt));
                l.gLow  = s.freeze ? 1.0f : juce::jmin (0.99995f, gainFor (rt * s.bassRatio));
                l.gHigh = s.freeze ? 1.0f : juce::jmin (0.99995f, gainFor (rt * juce::jmax (0.05f, s.highRatio)));
            }
            aLow  = (float) std::exp (-juce::MathConstants<double>::twoPi * 250.0 / sr);
            aHigh = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jlimit (500.0, 0.45 * sr, (double) s.highCut) / sr);
            apGain = 0.75f * juce::jlimit (0.0f, 1.0f, s.diffusion);
            modSamples = s.modDepth * (s.freeze ? 3.0f : 14.0f) * (float) (sr / 48000.0);   // Freeze: kaum Modulation -> kaum Verlust
            modInc = s.modRate / (float) sr;
        }

        // nur Hallanteil (wet); in und out dürfen nicht dieselben Puffer sein
        void process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
        {
            const bool freeze = current.freeze;
            const float w = juce::jlimit (0.0f, 1.0f, current.width);

            for (int s = 0; s < n; ++s)
            {
                float xl = freeze ? 0.0f : inL[s];
                float xr = freeze ? 0.0f : inR[s];
                for (int a = 0; a < 4; ++a) { xl = allpass[0][a].process (xl, apGain); xr = allpass[1][a].process (xr, apGain); }

                std::array<float, N> y;
                for (int i = 0; i < N; ++i)
                {
                    auto& l = lines[i];
                    lfoPhase[i] += modInc * (0.75f + 0.09f * (float) i);
                    if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                    const float d = l.length + modSamples * (1.0f + std::sin (lfoPhase[i] * juce::MathConstants<float>::twoPi));
                    float v = l.read (d);
                    if (freeze) { y[(size_t) i] = v; continue; }
                    // 3-Band-Dämpfung
                    l.lpLow  = v + aLow  * (l.lpLow  - v);
                    l.lpHigh = v + aHigh * (l.lpHigh - v);
                    const float low = l.lpLow, high = v - l.lpHigh, mid = l.lpHigh - l.lpLow;
                    y[(size_t) i] = low * l.gLow + mid * l.gMid + high * l.gHigh;
                }

                // Householder: y - 2/N * Summe
                float sum = 0.0f;
                for (float v : y) sum += v;
                sum *= 2.0f / (float) N;

                float oL = 0.0f, oR = 0.0f;
                for (int i = 0; i < N; ++i)
                {
                    const float v = y[(size_t) i];
                    oL += v * outSignL[i];
                    oR += v * outSignR[i];
                    lines[i].write (v - sum + ((i & 1) ? xr : xl) * 0.5f);
                }
                oL *= 0.35f; oR *= 0.35f;
                const float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR) * w;
                outL[s] = mid + side;
                outR[s] = mid - side;
            }
        }

    private:
        struct Line
        {
            std::vector<float> buf;
            int w = 0;
            float length = 1000.0f, gLow = 0.5f, gMid = 0.5f, gHigh = 0.5f, lpLow = 0.0f, lpHigh = 0.0f;
            float read (float delay) const noexcept
            {
                const int size = (int) buf.size();
                float rp = (float) w - delay;
                while (rp < 0.0f) rp += (float) size;
                const int i0 = (int) rp;
                const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
                const float fr = rp - (float) i0;
                return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
            }
            void write (float v) noexcept
            {
                buf[(size_t) w] = v;
                if (++w >= (int) buf.size()) w = 0;
            }
        };

        struct Allpass
        {
            std::vector<float> buf;
            int idx = 0;
            float process (float x, float g) noexcept
            {
                const float d = buf[(size_t) idx];
                const float v = x + g * d;
                buf[(size_t) idx] = v;
                if (++idx >= (int) buf.size()) idx = 0;
                return d - g * v;
            }
        };

        static constexpr float baseMs[N] = { 31.7f, 37.3f, 41.9f, 47.1f, 53.9f, 59.3f, 67.1f, 73.7f };
        static constexpr float apMs[4]   = { 4.77f, 3.59f, 12.73f, 9.31f };
        static constexpr float outSignL[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
        static constexpr float outSignR[N] = { 1, 1, -1, 1, 1, -1, -1, 1 };

        double sr = 48000.0;
        Line lines[N];
        Allpass allpass[2][4];
        float lfoPhase[N] {};
        float aLow = 0.0f, aHigh = 0.0f, apGain = 0.5f, modSamples = 0.0f, modInc = 0.0f;
        Settings current;
    };
}

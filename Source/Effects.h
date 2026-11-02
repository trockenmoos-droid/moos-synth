#pragma once

#include <juce_dsp/juce_dsp.h>

#ifndef SHIMMER_FB
 #define SHIMMER_FB 0.6f
#endif

// 12-dB-Hochpass (TPT-SVF), stereo
class HighPass
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { for (auto& s : st) s = {}; }

    void process (juce::AudioBuffer<float>& buf, int start, int n, float cutoffHz)
    {
        if (cutoffHz <= 20.5f) { reset(); return; }
        const float g  = (float) std::tan (juce::MathConstants<double>::pi * juce::jmin (cutoffHz, (float) (0.45 * sr)) / sr);
        const float k  = 1.4142f;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        for (int ch = 0; ch < juce::jmin (2, buf.getNumChannels()); ++ch)
        {
            auto* d = buf.getWritePointer (ch, start);
            auto& s = st[ch];
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                const float v3 = x - s.ic2;
                const float v1 = a1 * s.ic1 + a2 * v3;
                const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
                s.ic1 = 2.0f * v1 - s.ic1;
                s.ic2 = 2.0f * v2 - s.ic2;
                d[i] = x - k * v1 - v2;
            }
        }
    }

private:
    struct State { float ic1 = 0, ic2 = 0; };
    State st[2];
    double sr = 44100.0;
};

// Stereo-Ping-Pong-Delay mit gedämpfter Rückkopplung
class PingPongDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 2.1) + 4;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        w = 0; lpL = lpR = 0.0f;
        timeSm.reset (sr, 0.25);
        timeSm.setCurrentAndTargetValue ((float) (0.375 * sr));
    }

    void process (juce::AudioBuffer<float>& buf, int start, int n, float timeMs, float fb, float mix)
    {
        timeSm.setTargetValue (juce::jlimit (1.0f, (float) (size - 3), timeMs * 0.001f * (float) sr));
        auto* L = buf.getWritePointer (0, start);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1, start) : L;
        const float damp = 0.35f;

        for (int i = 0; i < n; ++i)
        {
            const float d = timeSm.getNextValue();
            const float rl = read (bufL, d), rr = read (bufR, d);
            lpL += damp * (rl - lpL);
            lpR += damp * (rr - lpR);

            const float in = 0.5f * (L[i] + R[i]);
            bufL[(size_t) w] = in + fb * lpR;          // Ping-Pong: Kreuz-Rückkopplung
            bufR[(size_t) w] = fb * lpL;
            if (++w >= size) w = 0;

            L[i] += mix * rl;
            if (R != L) R[i] += mix * rr;
        }
    }

private:
    std::vector<float> bufL, bufR;
    int size = 0, w = 0;
    double sr = 44100.0;
    float lpL = 0, lpR = 0;
    juce::SmoothedValue<float> timeSm;

    float read (const std::vector<float>& b, float delay) const noexcept
    {
        float rp = (float) w - delay;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float fr = rp - (float) i0;
        return b[(size_t) i0] + fr * (b[(size_t) i1] - b[(size_t) i0]);
    }
};

// Oktav-Pitchshifter (zwei überblendete Leseköpfe) – für den Shimmer
class OctaveShifter
{
public:
    void reset() { std::fill (std::begin (buf), std::end (buf), 0.0f); w = 0; phase = 0.0f; }

    float process (float x) noexcept
    {
        buf[w] = x;
        const float out = tap (phase) * win (phase) + tap (wrap (phase + 0.5f)) * win (wrap (phase + 0.5f));
        phase += 1.0f / (float) window;          // Verzögerung schrumpft 1 Sample/Sample -> doppelte Geschwindigkeit
        if (phase >= 1.0f) phase -= 1.0f;
        w = (w + 1) & (size - 1);
        return out;
    }

private:
    static constexpr int size = 8192, window = 2048;
    float buf[size] {};
    int w = 0;
    float phase = 0.0f;

    static float wrap (float p) noexcept { return p >= 1.0f ? p - 1.0f : p; }
    static float win (float p) noexcept  { const float s = std::sin (juce::MathConstants<float>::pi * p); return s * s; }

    float tap (float p) const noexcept
    {
        const float delay = (1.0f - p) * (float) window + 2.0f;
        float rp = (float) w - delay;
        if (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) & (size - 1);
        const float fr = rp - (float) i0;
        return buf[i0] + fr * (buf[i1] - buf[i0]);
    }
};

// Hall mit optionalem Shimmer (Oktave nach oben in der Rückkopplung)
class ShimmerReverb
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) chunk, 2 };
        reverb.prepare (spec);
        reverb.reset();
        wet.setSize (2, chunk);
        fb.setSize (2, chunk);
        fb.clear();
        for (auto& p : shifters) p.reset();
        lp[0] = lp[1] = hp[0] = hp[1] = 0.0f;
        juce::ignoreUnused (maxBlock);
    }

    void process (juce::AudioBuffer<float>& buf, int start, int n, float size, float mix, float shimmer)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 0.97f, size);
        rp.damping  = 0.45f;
        rp.wetLevel = 1.0f / 3.0f;     // JUCE skaliert intern x3 -> so etwa Einheitsverstärkung
        rp.dryLevel = 0.0f;
        rp.width    = 1.0f;
        reverb.setParameters (rp);

        for (int off = 0; off < n; off += chunk)
        {
            const int m = juce::jmin (chunk, n - off);
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto* src = buf.getReadPointer (juce::jmin (ch, buf.getNumChannels() - 1), start + off);
                auto* w = wet.getWritePointer (ch);
                const auto* f = fb.getReadPointer (ch);
                for (int i = 0; i < m; ++i) w[i] = src[i] + f[i];
            }

            juce::dsp::AudioBlock<float> blk (wet.getArrayOfWritePointers(), 2, (size_t) m);
            reverb.process (juce::dsp::ProcessContextReplacing<float> (blk));

            for (int ch = 0; ch < 2; ++ch)
            {
                const auto* w = wet.getReadPointer (ch);
                auto* f = fb.getWritePointer (ch);
                for (int i = 0; i < m; ++i)
                {
                    const float up = shifters[ch].process (w[i]);
                    lp[ch] += 0.25f * (up - lp[ch]);              // Höhen im Shimmer bändigen
                    hp[ch] += 0.02f * (lp[ch] - hp[ch]);                  // Tiefen aus der Schleife nehmen
                    f[i] = std::tanh ((lp[ch] - hp[ch]) * shimmer * SHIMMER_FB);
                }
                if (m < chunk) juce::FloatVectorOperations::clear (f + m, chunk - m);
            }

            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                auto* d = buf.getWritePointer (ch, start + off);
                const auto* w = wet.getReadPointer (juce::jmin (ch, 1));
                for (int i = 0; i < m; ++i)
                    d[i] = d[i] * (1.0f - 0.5f * mix) + w[i] * mix * 3.0f;
            }
        }
    }

private:
    static constexpr int chunk = 256;
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wet, fb;
    OctaveShifter shifters[2];
    float lp[2] {}, hp[2] {};
};

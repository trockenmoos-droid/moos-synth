#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// Bank aus 8 Wellenformen ("Frames"), zwischen denen stufenlos gemorpht wird.
// Jeder Frame existiert in 11 bandbegrenzten Versionen (Mipmaps), damit hohe
// Töne nicht aliasen.
class WavetableBank
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numFrames = 8;
    static constexpr int numLevels = 11;

    WavetableBank() { build(); }

    static int levelForFrequency (double freq, double sampleRate)
    {
        for (int k = 0; k < numLevels; ++k)
            if (maxHarmonicForLevel (k) * freq < 0.5 * sampleRate)
                return k;
        return numLevels - 1;
    }

    // Zeiger auf tableSize + 1 Samples (letztes = Kopie des ersten, für Interpolation)
    const float* get (int level, int frame) const noexcept
    {
        return data.data() + (size_t) (level * numFrames + frame) * (size_t) (tableSize + 1);
    }

    static const char* frameName (int frame)
    {
        static const char* names[numFrames] = { "Sine", "Triangle", "Saw", "Square",
                                                "Voice", "Organ", "Glass", "Dust" };
        return names[juce::jlimit (0, numFrames - 1, frame)];
    }

private:
    std::vector<float> data;

    static int maxHarmonicForLevel (int k)
    {
        return juce::jmin (tableSize / 2 - 1, (tableSize / 2) >> k);
    }

    static void spectrumForFrame (int frame, std::vector<float>& amp)
    {
        const int H = tableSize / 2;
        amp.assign ((size_t) H, 0.0f);
        juce::Random rng (4711);

        for (int h = 1; h < H; ++h)
        {
            const float fh = (float) h;
            float a = 0.0f;

            switch (frame)
            {
                case 0: a = (h == 1) ? 1.0f : 0.0f; break;                                          // Sinus
                case 1: a = (h % 2 == 1) ? (((h / 2) % 2 == 0) ? 1.0f : -1.0f) / (fh * fh) : 0.0f; break; // Dreieck
                case 2: a = 1.0f / fh; break;                                                       // Sägezahn
                case 3: a = (h % 2 == 1) ? 1.0f / fh : 0.0f; break;                                 // Rechteck
                case 4:                                                                             // Formant ("Ah")
                    a = (1.0f / fh) * (0.25f + 2.2f * std::exp (-(fh - 7.0f) * (fh - 7.0f) / 8.0f)
                                              + 1.6f * std::exp (-(fh - 17.0f) * (fh - 17.0f) / 18.0f));
                    break;
                case 5:                                                                             // Orgel-Zugriegel
                    switch (h)
                    {
                        case 1: a = 1.0f; break;   case 2: a = 0.7f; break;   case 3: a = 0.5f; break;
                        case 4: a = 0.6f; break;   case 6: a = 0.35f; break;  case 8: a = 0.4f; break;
                        case 10: a = 0.2f; break;  case 12: a = 0.25f; break; case 16: a = 0.2f; break;
                        default: a = 0.0f;
                    }
                    break;
                case 6:                                                                             // Glas
                    a = (1.0f / std::sqrt (fh)) * ((h % 2 == 0) ? 1.0f : 0.25f) * std::exp (-fh / 60.0f);
                    break;
                case 7:                                                                             // Dust (zufällig)
                default:
                    a = rng.nextFloat() / std::pow (fh, 0.8f) * ((h % 3 == 0) ? 0.2f : 1.0f);
                    break;
            }
            amp[(size_t) h] = a;
        }
    }

    void build()
    {
        constexpr int order = 11;
        static_assert ((1 << order) == tableSize, "tableSize muss 2^order sein");

        juce::dsp::FFT fft (order);
        data.assign ((size_t) (numLevels * numFrames * (tableSize + 1)), 0.0f);

        std::vector<float> amp;
        std::vector<juce::dsp::Complex<float>> in ((size_t) tableSize), out ((size_t) tableSize);

        for (int f = 0; f < numFrames; ++f)
        {
            spectrumForFrame (f, amp);

            for (int k = 0; k < numLevels; ++k)
            {
                std::fill (in.begin(), in.end(), juce::dsp::Complex<float> (0.0f, 0.0f));
                const int maxH = maxHarmonicForLevel (k);

                for (int h = 1; h <= maxH; ++h)
                {
                    in[(size_t) h]               = { 0.0f, -amp[(size_t) h] };
                    in[(size_t) (tableSize - h)] = { 0.0f,  amp[(size_t) h] };
                }

                fft.perform (in.data(), out.data(), true);

                float* dst = data.data() + (size_t) (k * numFrames + f) * (size_t) (tableSize + 1);
                float peak = 0.0f;
                for (int i = 0; i < tableSize; ++i)
                {
                    dst[i] = out[(size_t) i].real();
                    peak = juce::jmax (peak, std::abs (dst[i]));
                }
                if (peak > 0.0f)
                    for (int i = 0; i < tableSize; ++i)
                        dst[i] /= peak;
                dst[tableSize] = dst[0];
            }
        }
    }
};

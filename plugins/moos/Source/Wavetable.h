#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <cmath>

// Eine Bank aus N Wellenformen ("Frames", 1..64), zwischen denen gemorpht wird.
// Jeder Frame liegt in 11 bandbegrenzten Mipmap-Stufen vor (kein Aliasing).
class WavetableBank
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int fftOrder  = 11;
    static constexpr int numLevels = 11;
    static constexpr int maxFrames = 64;

    using Spectrum = std::vector<juce::dsp::Complex<float>>;   // tableSize Bins (nur 1..N/2-1 genutzt)

    explicit WavetableBank (const std::vector<Spectrum>& spectra, juce::String bankName)
        : name (std::move (bankName))
    {
        build (spectra);
    }

    int getNumFrames() const noexcept           { return numFrames; }
    const juce::String& getName() const noexcept { return name; }
    juce::String frameName (int f) const
    {
        if (frameNames.size() == numFrames) return frameNames[juce::jlimit (0, numFrames - 1, f)];
        return juce::String (juce::jlimit (0, numFrames - 1, f) + 1);
    }

    static int levelForFrequency (double freq, double sampleRate) noexcept
    {
        for (int k = 0; k < numLevels; ++k)
            if (maxHarmonicForLevel (k) * freq < 0.5 * sampleRate)
                return k;
        return numLevels - 1;
    }

    // tableSize + 1 Samples (letztes = Kopie des ersten)
    const float* get (int level, int frame) const noexcept
    {
        return data.data() + (size_t) (level * numFrames + frame) * (size_t) (tableSize + 1);
    }

    //==========================================================================
    static std::unique_ptr<WavetableBank> createFactory()
    {
        std::vector<Spectrum> spectra;
        for (int f = 0; f < 8; ++f)
            spectra.push_back (factorySpectrum (f));
        auto b = std::make_unique<WavetableBank> (spectra, "Moos Basis");
        b->frameNames = { "Sine", "Triangle", "Saw", "Square", "Voice", "Organ", "Glass", "Dust" };
        return b;
    }

    // Lädt eine WAV/AIFF-Datei:
    //  - Länge = k * 2048 Samples (2..64)  -> klassische Wavetable (Serum/Vital-Format)
    //  - sonst (z. B. Field Recording)      -> 16 Ausschnitte gleichmäßig über die Datei verteilt
    static std::unique_ptr<WavetableBank> createFromFile (const juce::File& file, juce::String& error)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr) { error = "Datei kann nicht gelesen werden"; return nullptr; }

        const auto maxLen = (juce::int64) tableSize * 4096;
        const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
        if (len < 64) { error = "Datei ist zu kurz"; return nullptr; }

        juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
        reader->read (&buf, 0, len, 0, true, true);

        std::vector<float> mono ((size_t) len, 0.0f);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int i = 0; i < len; ++i)
                mono[(size_t) i] += buf.getSample (ch, i) / (float) buf.getNumChannels();

        std::vector<std::vector<float>> frames;

        if (len % tableSize == 0 && len / tableSize >= 1 && len / tableSize <= maxFrames)
        {
            for (int f = 0; f < len / tableSize; ++f)
                frames.emplace_back (mono.begin() + f * tableSize, mono.begin() + (f + 1) * tableSize);
        }
        else if (len <= tableSize)
        {
            // kurzer Schnipsel: als ein Zyklus auf 2048 Samples strecken
            std::vector<float> fr ((size_t) tableSize);
            for (int i = 0; i < tableSize; ++i)
            {
                const double pos = (double) i * (len - 1) / (tableSize - 1);
                const int i0 = (int) pos;
                const int i1 = juce::jmin (i0 + 1, len - 1);
                fr[(size_t) i] = mono[(size_t) i0] + (float) (pos - i0) * (mono[(size_t) i1] - mono[(size_t) i0]);
            }
            frames.push_back (fr);
        }
        else
        {
            const int n = 16;
            for (int f = 0; f < n; ++f)
            {
                const int start = (int) ((double) f * (len - tableSize) / (n - 1));
                std::vector<float> fr (mono.begin() + start, mono.begin() + start + tableSize);
                // Enden weich überblenden, damit der Zyklus nahtlos loopt
                const int fade = 256;
                // Am Ende Richtung der Samples *vor* dem Fensterstart blenden: dann geht
                // das Ende stetig in den Anfang über (fr[N-1] ≈ mono[start-1] -> fr[0] = mono[start]).
                for (int i = 0; i < fade; ++i)
                {
                    const float t = (float) (i + 1) / fade;
                    const int preIdx = start - fade + i;
                    const float pre  = preIdx >= 0 ? mono[(size_t) preIdx] : fr[0];
                    auto& s = fr[(size_t) (tableSize - fade + i)];
                    s = s * (1.0f - t) + pre * t;
                }
                frames.push_back (fr);
            }
        }

        // Frames in Spektren umwandeln
        juce::dsp::FFT fft (fftOrder);
        std::vector<Spectrum> spectra;
        for (auto& fr : frames)
        {
            Spectrum in ((size_t) tableSize), out ((size_t) tableSize);
            for (int i = 0; i < tableSize; ++i) in[(size_t) i] = { fr[(size_t) i], 0.0f };
            fft.perform (in.data(), out.data(), false);
            out[0] = { 0.0f, 0.0f };                       // DC entfernen
            spectra.push_back (out);
        }

        auto bank = std::make_unique<WavetableBank> (spectra, file.getFileNameWithoutExtension());
        if (bank->silent) { error = "Datei enthält nur Stille"; return nullptr; }
        return bank;
    }

private:
    int numFrames = 0;
    bool silent = false;
    juce::String name;
    juce::StringArray frameNames;
    std::vector<float> data;

    static int maxHarmonicForLevel (int k) noexcept
    {
        return juce::jmin (tableSize / 2 - 1, (tableSize / 2) >> k);
    }

    static Spectrum factorySpectrum (int frame)
    {
        Spectrum s ((size_t) tableSize, { 0.0f, 0.0f });
        juce::Random rng (4711);

        for (int h = 1; h < tableSize / 2; ++h)
        {
            const float fh = (float) h;
            float a = 0.0f;
            switch (frame)
            {
                case 0: a = (h == 1) ? 1.0f : 0.0f; break;
                case 1: a = (h % 2 == 1) ? (((h / 2) % 2 == 0) ? 1.0f : -1.0f) / (fh * fh) : 0.0f; break;
                case 2: a = 1.0f / fh; break;
                case 3: a = (h % 2 == 1) ? 1.0f / fh : 0.0f; break;
                case 4: a = (1.0f / fh) * (0.25f + 2.2f * std::exp (-(fh - 7.0f) * (fh - 7.0f) / 8.0f)
                                                 + 1.6f * std::exp (-(fh - 17.0f) * (fh - 17.0f) / 18.0f)); break;
                case 5:
                    switch (h)
                    {
                        case 1: a = 1.0f; break;   case 2: a = 0.7f; break;   case 3: a = 0.5f; break;
                        case 4: a = 0.6f; break;   case 6: a = 0.35f; break;  case 8: a = 0.4f; break;
                        case 10: a = 0.2f; break;  case 12: a = 0.25f; break; case 16: a = 0.2f; break;
                        default: a = 0.0f;
                    }
                    break;
                case 6: a = (1.0f / std::sqrt (fh)) * ((h % 2 == 0) ? 1.0f : 0.25f) * std::exp (-fh / 60.0f); break;
                default: a = rng.nextFloat() / std::pow (fh, 0.8f) * ((h % 3 == 0) ? 0.2f : 1.0f); break;
            }
            // Sinus-Phase: X[h] = -i*a, X[N-h] = +i*a
            s[(size_t) h]              = { 0.0f, -a };
            s[(size_t) (tableSize - h)] = { 0.0f, a };
        }
        return s;
    }

    void build (const std::vector<Spectrum>& spectra)
    {
        numFrames = juce::jlimit (1, maxFrames, (int) spectra.size());
        juce::dsp::FFT fft (fftOrder);
        data.assign ((size_t) (numLevels * numFrames * (tableSize + 1)), 0.0f);

        Spectrum in ((size_t) tableSize), out ((size_t) tableSize);
        float globalPeak = 0.0f;

        for (int f = 0; f < numFrames; ++f)
        {
            const auto& spec = spectra[(size_t) f];
            float level0Peak = 0.0f;

            for (int k = 0; k < numLevels; ++k)
            {
                std::fill (in.begin(), in.end(), juce::dsp::Complex<float> (0.0f, 0.0f));
                const int maxH = maxHarmonicForLevel (k);
                for (int h = 1; h <= maxH; ++h)
                {
                    in[(size_t) h]               = spec[(size_t) h];
                    in[(size_t) (tableSize - h)] = spec[(size_t) (tableSize - h)];
                }
                fft.perform (in.data(), out.data(), true);

                float* dst = data.data() + (size_t) (k * numFrames + f) * (size_t) (tableSize + 1);
                float peak = 0.0f;
                for (int i = 0; i < tableSize; ++i)
                {
                    dst[i] = out[(size_t) i].real();
                    peak = juce::jmax (peak, std::abs (dst[i]));
                }
                if (k == 0) level0Peak = peak;
                // gleiche Normalisierung für alle Stufen eines Frames -> keine Lautstärkesprünge
                const float norm = level0Peak > 1.0e-9f ? 1.0f / level0Peak : 0.0f;
                for (int i = 0; i < tableSize; ++i) dst[i] *= norm;
                dst[tableSize] = dst[0];
            }
            globalPeak = juce::jmax (globalPeak, level0Peak);
        }
        silent = globalPeak < 1.0e-6f;
    }
};

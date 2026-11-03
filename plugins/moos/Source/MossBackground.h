#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Erzeugt eine fotorealistisch angehauchte Moos-Textur (prozedural, kein Fremdmaterial)
// oder lädt ein eigenes Foto. Liefert zusätzlich eine weichgezeichnete Version
// für die "Milchglas"-Panels.
namespace MossBackground
{
    constexpr int width = 1800, height = 1170;

    inline float hash (int x, int y, int seed) noexcept
    {
        juce::uint32 h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u + (juce::uint32) seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffff) / (float) 0xffffff;
    }

    inline float valueNoise (float x, float y, int seed) noexcept
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed);
        const float c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
        return juce::jmap (fy, juce::jmap (fx, a, b), juce::jmap (fx, c, d));
    }

    inline float fbm (float x, float y, int octaves, int seed) noexcept
    {
        float sum = 0.0f, amp = 0.5f, norm = 0.0f;
        for (int o = 0; o < octaves; ++o)
        {
            sum += amp * valueNoise (x, y, seed + o * 17);
            norm += amp;
            x *= 2.03f; y *= 2.03f; amp *= 0.5f;
        }
        return sum / norm;
    }

    inline juce::Image generateMoss()
    {
        const int W = width, H = height;
        std::vector<float> hf ((size_t) (W * H)), dryMap ((size_t) (W * H));

        // 1) Höhenfeld: gewölbte Moospolster (zufällig verteilte Kuppen) + sanfte Wellen
        const float cell = 120.0f;
        const int gw = (int) (W / cell) + 3, gh = (int) (H / cell) + 3;
        std::vector<juce::Point<float>> centres ((size_t) (gw * gh));
        std::vector<float> radii ((size_t) (gw * gh));
        for (int gy = 0; gy < gh; ++gy)
            for (int gx = 0; gx < gw; ++gx)
            {
                const int i = gy * gw + gx;
                centres[(size_t) i] = { (gx - 1 + hash (gx, gy, 3)) * cell, (gy - 1 + hash (gx, gy, 5)) * cell };
                radii[(size_t) i]   = cell * (0.75f + 0.6f * hash (gx, gy, 9));
            }

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const int cx = (int) (x / cell) + 1, cy = (int) (y / cell) + 1;
                float h = 0.0f;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int i = juce::jlimit (0, gh - 1, cy + dy) * gw + juce::jlimit (0, gw - 1, cx + dx);
                        const float d = centres[(size_t) i].getDistanceFrom ({ (float) x, (float) y }) / radii[(size_t) i];
                        if (d < 1.0f) h = juce::jmax (h, std::sqrt (1.0f - d * d));
                    }
                h = 0.75f * h + 0.35f * fbm (x / 300.0f, y / 300.0f, 3, 21) + 0.12f * fbm (x / 25.0f, y / 25.0f, 2, 33);
                hf[(size_t) (y * W + x)] = h;
                dryMap[(size_t) (y * W + x)] = juce::jlimit (0.0f, 1.0f, (fbm (x / 500.0f + 3.0f, y / 500.0f, 3, 41) - 0.5f) * 3.2f);
            }

        // 2) Licht: Lambert aus dem Gefälle des Höhenfelds, Licht von links oben
        std::vector<float> shade ((size_t) (W * H));
        const float lnorm = std::sqrt (0.55f * 0.55f + 0.65f * 0.65f + 0.75f * 0.75f);
        const float lx = -0.55f / lnorm, ly = -0.65f / lnorm, lz = 0.75f / lnorm;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const int x0 = juce::jmax (0, x - 2), x1 = juce::jmin (W - 1, x + 2);
                const int y0 = juce::jmax (0, y - 2), y1 = juce::jmin (H - 1, y + 2);
                const float gx = (hf[(size_t) (y * W + x1)] - hf[(size_t) (y * W + x0)]) * 30.0f;
                const float gy = (hf[(size_t) (y1 * W + x)] - hf[(size_t) (y0 * W + x)]) * 30.0f;
                const float nl = std::sqrt (gx * gx + gy * gy + 1.0f);
                const float lam = juce::jmax (0.0f, (-gx * lx - gy * ly + lz) / nl);
                const float ao  = juce::jlimit (0.0f, 1.0f, hf[(size_t) (y * W + x)] * 1.3f);
                shade[(size_t) (y * W + x)] = (0.18f + 0.95f * lam) * (0.25f + 0.75f * ao);
            }

        // 3) Grundfarbe (dunkle Tiefe zwischen den Blättchen)
        std::vector<float> R ((size_t) (W * H)), G ((size_t) (W * H)), B ((size_t) (W * H));
        for (size_t i = 0; i < R.size(); ++i)
        {
            const float s = shade[i] * 0.35f;
            R[i] = (18.0f + 30.0f * dryMap[i]) * s;
            G[i] = (40.0f + 10.0f * dryMap[i]) * s;
            B[i] = 10.0f * s;
        }

        // 4) Hunderttausende Moospflänzchen: kleine Sterne aus Blättchen, Spitzen heller
        juce::Random rng (2026);
        auto plot = [&] (float fx, float fy, float r, float g, float b, float a)
        {
            const int x = (int) fx, y = (int) fy;
            if (x < 0 || y < 0 || x >= W || y >= H) return;
            const size_t i = (size_t) (y * W + x);
            R[i] += (r - R[i]) * a; G[i] += (g - G[i]) * a; B[i] += (b - B[i]) * a;
        };

        const int sprigs = 520000;
        for (int n = 0; n < sprigs; ++n)
        {
            const float x = rng.nextFloat() * W, y = rng.nextFloat() * H;
            const size_t pi = (size_t) ((int) y * W + (int) x);
            const float h = hf[pi];
            if (rng.nextFloat() > h * 1.6f) continue;                 // weniger in den Ritzen

            const float sh  = shade[pi] * (0.75f + 0.5f * rng.nextFloat());
            const float dry = dryMap[pi];
            // Farbe: von sattem Grün bis trockenem Goldbraun
            const float baseR = juce::jmap (dry, 70.0f, 165.0f);
            const float baseG = juce::jmap (dry, 125.0f, 135.0f);
            const float baseB = juce::jmap (dry, 25.0f, 55.0f);
            const float hue   = rng.nextFloat() * 0.35f;               // etwas Gelbgrün einstreuen

            const int leaves = 5 + rng.nextInt (4);
            const float len  = 3.0f + rng.nextFloat() * 5.5f;
            const float rot  = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            for (int l = 0; l < leaves; ++l)
            {
                const float ang = rot + (float) l / leaves * juce::MathConstants<float>::twoPi + (rng.nextFloat() - 0.5f) * 0.5f;
                const float ll  = len * (0.6f + 0.5f * rng.nextFloat());
                const float dx = std::cos (ang), dy = std::sin (ang);
                const float bend = (rng.nextFloat() - 0.5f) * 0.25f;
                for (float t = 0.0f; t <= ll; t += 0.7f)
                {
                    const float u = t / ll;
                    const float bright = sh * (0.65f + 0.6f * u);       // Spitzen fangen mehr Licht
                    plot (x + dx * t - dy * bend * t * u, y + dy * t + dx * bend * t * u,
                          juce::jmin (255.0f, (baseR + hue * 60.0f) * bright),
                          juce::jmin (255.0f, (baseG + hue * 40.0f) * bright),
                          juce::jmin (255.0f, baseB * bright),
                          0.55f + 0.3f * u);
                }
            }
            // gelegentlich ein Sporenkapsel-Stiel (feine rotbraune Linie mit hellem Kopf)
            if (rng.nextFloat() < 0.004f * h)
            {
                const float ang = -juce::MathConstants<float>::halfPi + (rng.nextFloat() - 0.5f) * 1.2f;
                const float ll = 18.0f + rng.nextFloat() * 22.0f;
                for (float t = 0.0f; t <= ll; t += 0.6f)
                    plot (x + std::cos (ang) * t, y + std::sin (ang) * t, 120.0f * sh + 40.0f, 60.0f * sh + 20.0f, 30.0f, 0.7f);
                plot (x + std::cos (ang) * ll, y + std::sin (ang) * ll, 190.0f, 140.0f, 70.0f, 0.9f);
                plot (x + std::cos (ang) * ll + 1, y + std::sin (ang) * ll, 170.0f, 120.0f, 60.0f, 0.9f);
            }
        }

        juce::Image img (juce::Image::RGB, W, H, true);
        {
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const size_t i = (size_t) (y * W + x);
                    bd.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0.0f, 255.0f, R[i]),
                                                           (juce::uint8) juce::jlimit (0.0f, 255.0f, G[i]),
                                                           (juce::uint8) juce::jlimit (0.0f, 255.0f, B[i])));
                }
        }

        juce::Graphics g (img);
        juce::ColourGradient lightSpot (juce::Colour (0x28fff2c0), W * 0.2f, H * 0.1f,
                                        juce::Colour (0x00fff2c0), W * 0.8f, H * 0.8f, true);
        g.setGradientFill (lightSpot);
        g.fillAll();
        juce::ColourGradient vig (juce::Colour (0x00000000), W * 0.5f, H * 0.45f,
                                  juce::Colour (0xa0000000), 0.0f, 0.0f, true);
        g.setGradientFill (vig);
        g.fillAll();
        return img;
    }

    inline juce::Image fromPhoto (const juce::File& file)
    {
        auto photo = juce::ImageFileFormat::loadFrom (file);
        if (! photo.isValid()) return {};

        juce::Image img (juce::Image::RGB, width, height, true);
        juce::Graphics g (img);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (photo, juce::Rectangle<float> (0, 0, (float) width, (float) height),
                     juce::RectanglePlacement::fillDestination);
        g.fillAll (juce::Colour (0x40000000));   // etwas abdunkeln, damit die Regler lesbar bleiben
        return img;
    }

    inline juce::Image blurred (const juce::Image& src)
    {
        // Weichzeichnen über starkes Verkleinern + Vergrößern
        auto small = src.rescaled (src.getWidth() / 24, src.getHeight() / 24, juce::Graphics::highResamplingQuality);
        auto mid   = small.rescaled (src.getWidth() / 6, src.getHeight() / 6, juce::Graphics::highResamplingQuality);
        return mid.rescaled (src.getWidth() / 2, src.getHeight() / 2, juce::Graphics::highResamplingQuality);
    }

    // Erzeugt die Textur einmalig in einem Hintergrund-Thread; geteilt von allen Instanzen
    struct MossCache : private juce::Thread
    {
        MossCache() : juce::Thread ("MOOS moss texture") { startThread (juce::Thread::Priority::low); }
        ~MossCache() override { stopThread (15000); }

        bool get (juce::Image& image, juce::Image& blur)
        {
            const juce::ScopedLock sl (lock);
            if (! ready) return false;
            image = img; blur = blurImg;
            return true;
        }

    private:
        void run() override
        {
            auto i = generateMoss();
            auto b = blurred (i);
            const juce::ScopedLock sl (lock);
            img = i; blurImg = b; ready = true;
        }

        juce::CriticalSection lock;
        juce::Image img, blurImg;
        bool ready = false;
    };

    // Einstellung, die für alle MOOS-Instanzen gilt: Pfad zum eigenen Hintergrundfoto
    inline juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "MOOS";
        o.folderName          = "trockenmoos";
        o.filenameSuffix      = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }

    inline juce::String getPhotoPath()
    {
        juce::PropertiesFile f (settingsOptions());
        return f.getValue ("backgroundImage");
    }

    inline void setPhotoPath (const juce::String& path)
    {
        juce::PropertiesFile f (settingsOptions());
        if (path.isEmpty()) f.removeValue ("backgroundImage");
        else                f.setValue ("backgroundImage", path);
        f.saveIfNeeded();
    }
}

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <map>

// Prozedurale Landschafts-Texturen für die Plugin-Hintergründe (kein Fremdmaterial)
namespace tmk::tex
{
    constexpr int W = 1800, H = 1170;

    inline float hash (int x, int y, int seed) noexcept
    {
        juce::uint32 h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u + (juce::uint32) seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffff) / (float) 0xffffff;
    }

    inline float noise (float x, float y, int seed) noexcept
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
            sum += amp * noise (x, y, seed + o * 17);
            norm += amp;
            x *= 2.03f; y *= 2.03f; amp *= 0.5f;
        }
        return sum / norm;
    }

    // Puffer in Fließkomma-RGB, am Ende in ein Image
    struct RGB
    {
        std::vector<float> r, g, b;
        RGB() : r ((size_t) (W * H)), g ((size_t) (W * H)), b ((size_t) (W * H)) {}
        void set (int x, int y, juce::Colour c)
        {
            const size_t i = (size_t) (y * W + x);
            r[i] = c.getFloatRed(); g[i] = c.getFloatGreen(); b[i] = c.getFloatBlue();
        }
        void blend (int x, int y, float cr, float cg, float cb, float a)
        {
            if (x < 0 || y < 0 || x >= W || y >= H) return;
            const size_t i = (size_t) (y * W + x);
            r[i] += (cr - r[i]) * a; g[i] += (cg - g[i]) * a; b[i] += (cb - b[i]) * a;
        }
        juce::Image toImage() const
        {
            juce::Image img (juce::Image::RGB, W, H, true);
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const size_t i = (size_t) (y * W + x);
                    bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, r[i]),
                                                                          juce::jlimit (0.0f, 1.0f, g[i]),
                                                                          juce::jlimit (0.0f, 1.0f, b[i]), 1.0f));
                }
            return img;
        }
    };

    inline void vignette (juce::Image& img, float strength = 0.6f)
    {
        juce::Graphics g (img);
        juce::ColourGradient vig (juce::Colour (0x00000000), W * 0.5f, H * 0.45f,
                                  juce::Colours::black.withAlpha (strength), 0.0f, 0.0f, true);
        g.setGradientFill (vig);
        g.fillAll();
    }

    //==========================================================================
    // HALLSTATT: See in der Abenddämmerung, Bergkulissen, Spiegelung, Lichter am Ufer
    inline juce::Image lakeAtDusk()
    {
        RGB px;
        const int horizon = (int) (H * 0.56f);

        // Bergsilhouetten (3 Ebenen): Höhe pro x-Spalte
        auto ridge = [] (int x, float base, float amp, float scale, int seed)
        {
            const float fx = (float) x / scale;
            float r = fbm (fx, 0.5f, 5, seed);
            r = 1.0f - std::abs (r * 2.0f - 1.0f);          // gratige Spitzen
            return base - amp * (0.35f * r + 0.65f * fbm (fx * 0.35f, 1.5f, 3, seed + 3));
        };

        std::vector<float> r1 ((size_t) W), r2 ((size_t) W), r3 ((size_t) W);
        for (int x = 0; x < W; ++x)
        {
            const float u = (float) x / W;
            // steile Flanken links (über dem Ort) und rechts, Dachstein-Massiv hinten in der Mitte
            const float steepL = std::exp (-std::pow ((u - 0.10f) / 0.16f, 2.0f));
            const float steepR = std::exp (-std::pow ((u - 0.90f) / 0.13f, 2.0f));
            const float massif = std::exp (-std::pow ((u - 0.55f) / 0.22f, 2.0f));
            r1[(size_t) x] = ridge (x, (float) horizon - 60.0f - 250.0f * massif, 300.0f, 170.0f, 11) + 25.0f * (fbm (x / 18.0f, 3.0f, 3, 12) - 0.5f);
            r2[(size_t) x] = ridge (x, (float) horizon - 10.0f - 200.0f * (steepL + steepR), 220.0f, 150.0f, 23) + 18.0f * (fbm (x / 14.0f, 5.0f, 3, 24) - 0.5f);
            r3[(size_t) x] = ridge (x, (float) horizon + 2.0f - 330.0f * steepL - 260.0f * steepR, 150.0f, 110.0f, 37) + 14.0f * (fbm (x / 9.0f, 7.0f, 3, 38) - 0.5f);
        }

        const juce::Colour skyTop (0xff0c1733), skyMid (0xff3a5a86), skyGlow (0xffeba873);
        for (int y = 0; y < horizon; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float t = (float) y / horizon;
                auto c = t < 0.5f ? skyTop.interpolatedWith (skyMid, t / 0.5f)
                                  : skyMid.interpolatedWith (skyGlow, std::pow ((t - 0.5f) / 0.5f, 1.3f) * 0.9f);
                // Alpenglühen: warmes Licht hinter dem Massiv
                c = c.interpolatedWith (juce::Colour (0xfff3b88a), 0.35f * std::exp (-std::pow (((float) x / W - 0.6f) / 0.3f, 2.0f)) * t);
                // Schleierwolken
                const float cl = fbm (x / 420.0f, y / 90.0f, 4, 5);
                c = c.interpolatedWith (juce::Colour (0xff8a90a8), juce::jlimit (0.0f, 0.35f, (cl - 0.55f) * 1.4f) * (1.0f - t));
                // Sterne im oberen Himmel
                if (t < 0.45f && hash (x, y, 99) > 0.9993f) c = c.interpolatedWith (juce::Colours::white, 0.7f * (1.0f - t / 0.45f));
                float yy = (float) y;
                if (yy >= r1[(size_t) x]) c = juce::Colour (0xff4a5a76).interpolatedWith (juce::Colour (0xfff0e2dc),       // Fels mit Schnee, rosa angehaucht
                                                  juce::jlimit (0.0f, 1.0f, (fbm (x / 9.0f, y / 45.0f, 4, 7) - 0.42f) * 3.2f)
                                                  * juce::jlimit (0.0f, 1.0f, 1.0f - (yy - r1[(size_t) x]) / 180.0f));
                if (yy >= r2[(size_t) x]) c = juce::Colour (0xff22314a).interpolatedWith (juce::Colour (0xff3a4c66), fbm (x / 40.0f, y / 40.0f, 3, 9));
                if (yy >= r3[(size_t) x]) c = juce::Colour (0xff0e1a22).interpolatedWith (juce::Colour (0xff1f3331), fbm (x / 12.0f, y / 12.0f, 3, 13));
                // Dunst über dem Wasser
                const float mist = std::exp (-std::pow ((yy - horizon + 12.0f) / 28.0f, 2.0f)) * 0.3f;
                c = c.interpolatedWith (juce::Colour (0xff9fb2c8), mist);
                px.set (x, y, c);
            }

        // See: gespiegelte Landschaft mit Wellen
        for (int y = horizon; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float d = (float) (y - horizon);
                const float wave = std::sin (d * 0.25f + fbm (x / 180.0f, d / 12.0f, 3, 17) * 9.0f) * (1.5f + d * 0.035f);
                const int sx = juce::jlimit (0, W - 1, (int) (x + wave));
                const int sy = juce::jlimit (0, horizon - 1, (int) (horizon - d * 0.9f - 1 + wave * 0.4f));
                const size_t si = (size_t) (sy * W + sx);
                const float dark = 0.7f - 0.3f * juce::jlimit (0.0f, 1.0f, d / (H - horizon));
                float rr = px.r[si] * dark, gg = px.g[si] * dark * 1.02f, bb = px.b[si] * dark * 1.1f + 0.02f;
                // Glitzern
                const float glint = std::pow (juce::jmax (0.0f, fbm (x / 6.0f, d / 1.6f, 2, 23) - 0.62f) * 2.6f, 3.0f);
                rr += glint * 0.35f; gg += glint * 0.32f; bb += glint * 0.3f;
                px.r[(size_t) (y * W + x)] = rr; px.g[(size_t) (y * W + x)] = gg; px.b[(size_t) (y * W + x)] = bb;
            }

        // Ort am Ufer: Häuser mit Fenstern, Kirchturm, Spiegelungen der Lichter
        juce::Random rng (1599);
        auto rect = [&] (int x0, int y0, int x1, int y1, float r, float g, float b, float a)
        { for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) px.blend (x, y, r, g, b, a); };

        for (int i = 0; i < 46; ++i)
        {
            const int hx = (int) (W * (0.02f + rng.nextFloat() * 0.3f));
            const int hw = 14 + rng.nextInt (16), hh = 12 + rng.nextInt (16);
            const int hy = horizon - 4 - rng.nextInt (70) - (int) (40.0f * std::exp (-std::pow (((float) hx / W - 0.1f) / 0.08f, 2.0f)));
            rect (hx, hy - hh, hx + hw, hy, 0.12f, 0.12f, 0.14f, 0.92f);
            for (int k = 0; k < hw / 2; ++k) rect (hx - 2 + k, hy - hh - k / 2, hx + hw + 2 - k, hy - hh - k / 2 + 1, 0.08f, 0.07f, 0.08f, 0.92f);   // Dach
            if (rng.nextFloat() < 0.7f)
            {
                const int wx = hx + 3 + rng.nextInt (juce::jmax (1, hw - 8)), wy = hy - hh + 4 + rng.nextInt (juce::jmax (1, hh - 8));
                rect (wx, wy, wx + 3, wy + 4, 1.0f, 0.78f, 0.45f, 0.95f);
                for (int k = 0; k < 60; ++k)
                    px.blend (wx + 1 + (int) (std::sin (k * 0.8f) * 2.0f), 2 * horizon - wy + k * 2, 1.0f, 0.75f, 0.45f, 0.22f * (1.0f - k / 60.0f));
            }
        }
        {
            // Kirchturm mit spitzem Helm und erleuchtetem Fenster
            const int cx = (int) (W * 0.2f), base = horizon - 30;
            rect (cx - 9, base - 110, cx + 9, base, 0.11f, 0.11f, 0.13f, 0.97f);
            for (int k = 0; k < 70; ++k) rect (cx - 9 + k * 9 / 70, base - 110 - k, cx + 9 - k * 9 / 70, base - 109 - k, 0.09f, 0.09f, 0.11f, 0.97f);
            rect (cx - 3, base - 92, cx + 3, base - 80, 1.0f, 0.82f, 0.5f, 0.9f);
        }

        auto img = px.toImage();
        vignette (img, 0.55f);
        return img;
    }

    //==========================================================================
    // SOLELEITUNG: altes Lärchenholz der Rohrleitung, Salzkrusten
    inline juce::Image woodPipe()
    {
        RGB px;
        const juce::Colour dark (0xff2b1a0e), mid (0xff6b4526), light (0xffb98a55);

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const int plank = (int) (fy / 290.0f);
                const float py = std::fmod (fy, 290.0f);
                // Jahresringe, verzogen durch Rauschen und Äste
                float warp = fbm (fx / 400.0f, fy / 90.0f + plank * 7.0f, 4, 3 + plank) * 60.0f;
                for (int k = 0; k < 3; ++k)
                {
                    const float kx = hash (k, plank, 51) * W, ky = 60.0f + hash (k, plank, 52) * 170.0f;
                    const float dx = (fx - kx) / 60.0f, dy = (py - ky) / 26.0f;
                    warp += 30.0f * std::exp (-(dx * dx + dy * dy) * 0.5f) * (py > ky ? 1.0f : -1.0f);
                }
                const float rings = 0.5f + 0.5f * std::sin ((py + warp) * 0.33f + fbm (fx / 60.0f, py / 8.0f, 2, 9) * 2.0f);
                const float fibre = fbm (fx / 3.0f, py / 70.0f, 3, 21);
                float t = 0.25f + 0.45f * std::pow (rings, 2.5f) + 0.3f * fibre;
                auto c = t < 0.5f ? dark.interpolatedWith (mid, t * 2.0f) : mid.interpolatedWith (light, (t - 0.5f) * 2.0f);

                // Fugen zwischen den Planken, Rundung (Licht von oben)
                const float edge = juce::jmin (py, 290.0f - py);
                c = c.withMultipliedBrightness (0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, edge / 26.0f));
                c = c.withMultipliedBrightness (0.8f + 0.35f * std::sin (py / 290.0f * juce::MathConstants<float>::pi));
                if (edge < 3.0f) c = juce::Colour (0xff0e0805);

                // Salzkruste: weiß-graue Kristalle, sammeln sich unten an den Planken
                const float salt = fbm (fx / 38.0f, fy / 30.0f, 4, 77) + 0.5f * std::pow (py / 290.0f, 4.0f) - 0.66f;
                if (salt > 0.0f)
                {
                    const float s = juce::jlimit (0.0f, 0.8f, salt * 4.0f) * (0.35f + 0.65f * std::pow (hash (x, y, 3), 0.6f));
                    c = c.interpolatedWith (juce::Colour (0xffe8e4da), s);
                }
                px.set (x, y, c);
            }

        // Glitzernde Salzkristalle
        juce::Random rng (1595);
        for (int i = 0; i < 9000; ++i)
        {
            const int x = rng.nextInt (W), y = rng.nextInt (H);
            const float py = std::fmod ((float) y, 290.0f);
            if (fbm (x / 38.0f, y / 30.0f, 4, 77) + 0.5f * std::pow (py / 290.0f, 4.0f) - 0.64f < 0.0f) continue;
            const float a = 0.5f + 0.5f * rng.nextFloat();
            px.blend (x, y, 1.0f, 1.0f, 0.98f, a);
            px.blend (x + 1, y, 0.95f, 0.95f, 0.92f, a * 0.5f);
            px.blend (x, y + 1, 0.95f, 0.95f, 0.92f, a * 0.4f);
        }

        auto img = px.toImage();
        vignette (img, 0.6f);
        return img;
    }

    //==========================================================================
    // DACHSTEIN: Gletscherwand – Schichten im Eis, tiefe blaue Spalten, Schneekante, Lichtbrechung
    inline juce::Image glacierIce()
    {
        RGB px;
        const juce::Colour deep (0xff05263f), blue (0xff1b6d9a), turq (0xff63c7dc), pale (0xffbfe9f2), snow (0xfff4fbfe);

        // Spalten: zackige, nach unten breiter werdende Keile
        struct Crevasse { float x, width, depth, lean; int seed; };
        std::vector<Crevasse> cracks;
        juce::Random rr (1832);
        for (int i = 0; i < 5; ++i)
            cracks.push_back ({ (0.1f + 0.85f * (i + rr.nextFloat() * 0.5f) / 5.0f) * W, 12.0f + rr.nextFloat() * 22.0f,
                                0.28f + rr.nextFloat() * 0.35f, (rr.nextFloat() - 0.5f) * 0.6f, i * 13 + 3 });

        // Oberkante (Schnee) als Höhenlinie
        std::vector<float> top ((size_t) W);
        for (int x = 0; x < W; ++x)
            top[(size_t) x] = H * 0.1f + 70.0f * fbm (x / 260.0f, 0.3f, 4, 71) + 18.0f * (fbm (x / 25.0f, 2.0f, 2, 72) - 0.5f);

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const float t = fy / H;

                // Tiefe des Eises: oben hell, unten tiefblau, wolkige Transluzenz
                const float cloud = fbm (fx / 300.0f, fy / 220.0f, 5, 8);
                float d = juce::jlimit (0.0f, 1.0f, t * 0.95f + (cloud - 0.5f) * 0.55f);
                juce::Colour c = d < 0.33f ? pale.interpolatedWith (turq, d / 0.33f)
                               : d < 0.7f  ? turq.interpolatedWith (blue, (d - 0.33f) / 0.37f)
                                           : blue.interpolatedWith (deep, (d - 0.7f) / 0.3f);

                // Jahresschichten: leicht gewellte, horizontale Bänder, ab und zu dunkle Staublagen
                const float warp = fbm (fx / 500.0f, fy / 500.0f, 3, 15) * 120.0f + 30.0f * std::sin (fx / 380.0f);
                const float band = std::sin ((fy + warp) * 0.045f);
                c = c.withMultipliedBrightness (0.88f + 0.14f * band);
                const float dustLine = std::abs (std::sin ((fy + warp) * 0.0105f + 1.3f));
                if (dustLine < 0.05f) c = c.interpolatedWith (juce::Colour (0xff2f4550), 0.45f * (1.0f - dustLine / 0.05f) * (0.5f + cloud));

                // Spalten
                for (const auto& cr : cracks)
                {
                    const float yStart = top[(size_t) juce::jlimit (0, W - 1, (int) cr.x)];
                    const float len = cr.depth * H;
                    if (fy < yStart || fy > yStart + len) continue;
                    const float u = (fy - yStart) / len;                          // 0 oben .. 1 unten (Spitze)
                    const float halfW = cr.width * (1.0f - u) * (0.7f + 0.6f * fbm (fy / 30.0f, (float) cr.seed, 2, cr.seed));
                    const float cx = cr.x + cr.lean * (fy - yStart) + 25.0f * (fbm (fy / 60.0f, (float) cr.seed + 0.5f, 3, cr.seed + 1) - 0.5f);
                    const float dx = std::abs (fx - cx);
                    if (dx < halfW)
                    {
                        const float inner = dx / juce::jmax (1.0f, halfW);
                        // Gletscherblau: innen tief und satt, zum Rand hin leuchtend türkis
                        auto cc = juce::Colour (0xff0b4f86).interpolatedWith (juce::Colour (0xff06335c), 0.5f * (1.0f - inner) + 0.4f * u);
                        c = cc.interpolatedWith (juce::Colour (0xff7fe3f5), std::pow (inner, 4.0f) * 0.85f);
                    }
                    else if (dx < halfW + 6.0f)
                        c = c.interpolatedWith (juce::Colours::white, 0.35f * (1.0f - (dx - halfW) / 6.0f));
                }

                // Schneedecke und Wechte oben
                const float ty = top[(size_t) x];
                if (fy < ty)
                {
                    const float sky = fy / ty;
                    c = juce::Colour (0xff9fc4e0).interpolatedWith (juce::Colour (0xffdbe9f4), sky);   // Himmel hinter der Kante
                    if (fy > ty - 46.0f - 20.0f * fbm (fx / 80.0f, 4.0f, 3, 81))
                        c = snow.interpolatedWith (juce::Colour (0xffd2e4ee), fbm (fx / 15.0f, fy / 15.0f, 3, 82) * 0.6f);
                }
                else if (fy < ty + 14.0f)
                    c = c.interpolatedWith (snow, 0.75f * (1.0f - (fy - ty) / 14.0f));

                px.set (x, y, c);
            }

        // Luftblasen, feine Risse, Schneestaub
        juce::Random rng (2995);
        for (int i = 0; i < 5000; ++i)
        {
            const int x = rng.nextInt (W), y = rng.nextInt (H);
            const float a = 0.25f + 0.5f * rng.nextFloat();
            px.blend (x, y, 0.92f, 0.98f, 1.0f, a);
            if (rng.nextFloat() < 0.25f) px.blend (x + 1, y, 0.85f, 0.95f, 1.0f, a * 0.6f);
        }
        for (int i = 0; i < 160; ++i)
        {
            float x = (float) rng.nextInt (W), y = (float) rng.nextInt (H);
            float ang = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            const int len = 20 + rng.nextInt (90);
            for (int k = 0; k < len; ++k)
            {
                ang += (rng.nextFloat() - 0.5f) * 0.4f;
                x += std::cos (ang); y += std::sin (ang);
                px.blend ((int) x, (int) y, 0.9f, 0.97f, 1.0f, 0.22f);
            }
        }

        auto img = px.toImage();
        {
            juce::Graphics g (img);
            // Lichtstrahlen, die von oben rechts ins Eis fallen
            for (int i = 0; i < 6; ++i)
            {
                juce::Path ray;
                const float x0 = W * (0.45f + 0.1f * i), w0 = 40.0f + 25.0f * i;
                ray.addQuadrilateral (x0, 0, x0 + w0, 0, x0 - 400.0f + w0 * 3, (float) H, x0 - 400.0f, (float) H);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0x22ffffff), x0, 0, juce::Colour (0x00ffffff), x0 - 300.0f, H * 0.8f, false));
                g.fillPath (ray);
            }
        }
        vignette (img, 0.55f);
        return img;
    }

    //==========================================================================
    inline juce::Image blurred (const juce::Image& src)
    {
        auto small = src.rescaled (src.getWidth() / 24, src.getHeight() / 24, juce::Graphics::highResamplingQuality);
        auto mid   = small.rescaled (src.getWidth() / 6, src.getHeight() / 6, juce::Graphics::highResamplingQuality);
        return mid.rescaled (src.getWidth() / 2, src.getHeight() / 2, juce::Graphics::highResamplingQuality);
    }

    inline juce::Image fromPhoto (const juce::File& file)
    {
        auto photo = juce::ImageFileFormat::loadFrom (file);
        if (! photo.isValid()) return {};
        juce::Image img (juce::Image::RGB, W, H, true);
        juce::Graphics g (img);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (photo, juce::Rectangle<float> (0, 0, (float) W, (float) H), juce::RectanglePlacement::fillDestination);
        g.fillAll (juce::Colour (0x40000000));
        return img;
    }

    //==========================================================================
    // Erzeugt Texturen einmalig im Hintergrund; geteilt von allen Instanzen eines Plugins
    class Service : private juce::Thread
    {
    public:
        Service() : juce::Thread ("trockenmoos textures") {}
        ~Service() override { stopThread (20000); }

        // liefert true, sobald fertig; startet die Erzeugung beim ersten Aufruf
        bool get (const juce::String& name, std::function<juce::Image()> generator, juce::Image& img, juce::Image& blur)
        {
            const juce::ScopedLock sl (lock);
            if (auto it = done.find (name); it != done.end()) { img = it->second.first; blur = it->second.second; return true; }
            if (std::find_if (queue.begin(), queue.end(), [&] (auto& q) { return q.first == name; }) == queue.end())
                queue.push_back ({ name, std::move (generator) });
            if (! isThreadRunning()) startThread (juce::Thread::Priority::low);
            return false;
        }

    private:
        void run() override
        {
            for (;;)
            {
                std::pair<juce::String, std::function<juce::Image()>> job;
                {
                    const juce::ScopedLock sl (lock);
                    if (queue.empty()) return;
                    job = queue.front();
                }
                auto img = job.second();
                auto bl  = blurred (img);
                const juce::ScopedLock sl (lock);
                done[job.first] = { img, bl };
                queue.erase (queue.begin());
                if (threadShouldExit()) return;
            }
        }

        juce::CriticalSection lock;
        std::vector<std::pair<juce::String, std::function<juce::Image()>>> queue;
        std::map<juce::String, std::pair<juce::Image, juce::Image>> done;
    };

    // Pro Plugin gemerkter Pfad zu einem eigenen Hintergrundfoto
    inline juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "trockenmoos";
        o.folderName          = "trockenmoos";
        o.filenameSuffix      = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
    inline juce::String getPhotoPath (const juce::String& plugin)
    {
        juce::PropertiesFile f (settingsOptions());
        return f.getValue ("background_" + plugin);
    }
    inline void setPhotoPath (const juce::String& plugin, const juce::String& path)
    {
        juce::PropertiesFile f (settingsOptions());
        if (path.isEmpty()) f.removeValue ("background_" + plugin);
        else                f.setValue ("background_" + plugin, path);
        f.saveIfNeeded();
    }
}

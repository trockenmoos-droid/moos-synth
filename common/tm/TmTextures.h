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
    // MOOS: Moospolster mit Licht und Schatten, grüne und trockene Stellen
    inline juce::Image mossCushions()
    {
        
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


    //==========================================================================
    // SARSTEIN: Kalkwand – Bankung, Klüfte, Tintenstriche, Flechten
    inline juce::Image limestone()
    {
        RGB px;
        const float bedH = 150.0f;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                // leicht geneigte, gewellte Schichtung
                const float warp = 18.0f * fbm (fx / 400.0f, 0.5f, 3, 4) + fx * 0.06f;
                const float by = fy + warp;
                const int bed = (int) std::floor (by / bedH);
                const float inBed = by - bed * bedH;
                const float bedTone = hash (bed, 0, 91) * 0.12f;

                float lum = 0.56f + bedTone + 0.18f * (fbm (fx / 60.0f, fy / 60.0f, 5, 12) - 0.5f)
                          + 0.06f * (fbm (fx / 6.0f, fy / 6.0f, 2, 13) - 0.5f);
                // Bankungsfuge
                if (inBed < 5.0f) lum *= 0.35f + 0.13f * inBed;
                else if (inBed < 14.0f) lum *= 1.08f;                    // Oberkante fängt Licht
                // Klüfte: senkrechte Risse, pro Bank versetzt
                for (int j = 0; j < 4; ++j)
                {
                    const float jx = hash (bed, j, 92) * W + 30.0f * (fbm (fy / 40.0f, (float) j, 2, bed + j) - 0.5f);
                    const float d = std::abs (fx - jx);
                    if (d < 2.5f) lum *= 0.3f + 0.25f * d;
                    else if (d < 6.0f) lum *= 0.85f + 0.025f * d;
                }
                // Tintenstriche: dunkle Wasserstreifen von oben nach unten
                const float streak = fbm (fx / 22.0f, fy / 900.0f, 3, 31);
                const float wet = juce::jlimit (0.0f, 1.0f, (streak - 0.55f) * 4.0f) * (0.5f + 0.5f * fbm (fx / 200.0f, 1.0f, 2, 32));
                juce::Colour c = juce::Colour::fromFloatRGBA (lum * 1.0f, lum * 0.99f, lum * 0.95f, 1.0f);
                c = c.interpolatedWith (juce::Colour (0xff2a2e33), wet * 0.65f);
                // Flechten: orange und gelbgrün, fleckig
                const float l1 = fbm (fx / 14.0f, fy / 14.0f, 3, 51), l2 = fbm (fx / 18.0f + 9.0f, fy / 18.0f, 3, 52);
                if (l1 > 0.72f) c = c.interpolatedWith (juce::Colour (0xffc8742c), juce::jlimit (0.0f, 0.75f, (l1 - 0.72f) * 7.0f));
                if (l2 > 0.74f) c = c.interpolatedWith (juce::Colour (0xffa9b04a), juce::jlimit (0.0f, 0.7f, (l2 - 0.74f) * 7.0f));
                // Moos in den Fugen
                if (inBed < 9.0f && fbm (fx / 30.0f, (float) bed, 2, 61) > 0.6f) c = c.interpolatedWith (juce::Colour (0xff4b6a22), 0.6f);
                px.set (x, y, c);
            }
        auto img = px.toImage();
        {
            juce::Graphics g (img);
            juce::ColourGradient light (juce::Colour (0x30fff1d0), W * 0.8f, 0.0f, juce::Colour (0x00fff1d0), W * 0.2f, (float) H, false);
            g.setGradientFill (light);
            g.fillAll();
        }
        vignette (img, 0.6f);
        return img;
    }

    //==========================================================================
    // 5 FINGERS: Blick vom Krippenstein – Wolkenmeer im Morgenlicht, Gipfel ragen heraus
    inline juce::Image cloudSea()
    {
        RGB px;
        const int horizon = (int) (H * 0.42f);
        auto ridge = [] (int x, float base, float amp, float scale, int seed)
        {
            float r = fbm ((float) x / scale, 0.5f, 5, seed);
            r = 1.0f - std::abs (r * 2.0f - 1.0f);
            return base - amp * r;
        };
        std::vector<float> peaksFar ((size_t) W), peaksNear ((size_t) W);
        for (int x = 0; x < W; ++x)
        {
            const float u = (float) x / W;
            const float m1 = std::exp (-std::pow ((u - 0.28f) / 0.09f, 2.0f)), m2 = std::exp (-std::pow ((u - 0.72f) / 0.12f, 2.0f));
            peaksFar[(size_t) x]  = ridge (x, (float) horizon + 30.0f, 90.0f + 260.0f * m1 + 170.0f * m2, 180.0f, 5) + 12.0f * (fbm (x / 12.0f, 2.0f, 2, 6) - 0.5f);
            peaksNear[(size_t) x] = H * 1.08f - H * 0.62f * std::exp (-std::pow ((u - 0.97f) / 0.1f, 2.0f))
                                    - 30.0f * fbm ((float) x / 40.0f, 1.0f, 3, 9);
        }
        // geglättete Flankenneigung (für die Beleuchtung, ohne Streifen)
        std::vector<float> slopeFar ((size_t) W);
        for (int x = 0; x < W; ++x)
        {
            const int a = juce::jmax (0, x - 70), b = juce::jmin (W - 1, x + 70);
            slopeFar[(size_t) x] = (peaksFar[(size_t) b] - peaksFar[(size_t) a]) / (float) (b - a);
        }

        const juce::Colour zenith (0xff1c2d5a), mid (0xff8a6f9e), dawn (0xfff2a86e), sun (0xfffff1c8);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const float t = juce::jlimit (0.0f, 1.0f, fy / (horizon + 60.0f));
                juce::Colour c = t < 0.55f ? zenith.interpolatedWith (mid, t / 0.55f) : mid.interpolatedWith (dawn, (t - 0.55f) / 0.45f);
                const float sd = std::hypot ((fx - W * 0.62f) / W, (fy - horizon) / H);
                c = c.interpolatedWith (sun, juce::jlimit (0.0f, 1.0f, 0.9f - sd * 5.0f));

                // Gipfel (Morgenlicht von rechts: Ostflanken rosa, Schnee)
                if (fy >= peaksFar[(size_t) x])
                {
                    const float gx = fbm ((fx + 3.0f) / 45.0f, fy / 45.0f, 4, 18) - fbm ((fx - 3.0f) / 45.0f, fy / 45.0f, 4, 18);
                    const float lit = juce::jlimit (0.0f, 1.0f, 0.5f + slopeFar[(size_t) x] * 1.2f + gx * 6.0f);
                    const float snowAmt = juce::jlimit (0.0f, 1.0f, (fbm (fx / 14.0f, fy / 50.0f, 3, 17) - 0.42f) * 3.0f)
                                        * juce::jlimit (0.0f, 1.0f, 1.0f - (fy - peaksFar[(size_t) x]) / 220.0f);
                    const juce::Colour rock  = juce::Colour (0xff2e2c44).interpolatedWith (juce::Colour (0xff86636a), lit);
                    const juce::Colour snowC = juce::Colour (0xff8d93b8).interpolatedWith (juce::Colour (0xfffbd9cc), lit);
                    c = rock.interpolatedWith (snowC, snowAmt);
                }

                // Wolkenmeer
                const float cloudTop = H * 0.5f + 40.0f * (fbm (fx / 260.0f, 3.0f, 4, 23) - 0.5f);
                if (fy > cloudTop)
                {
                    const float depth = (fy - cloudTop) / (H - cloudTop);
                    // Perspektive: vorne große Wolkenballen, hinten kleine
                    const float sc = 0.3f + 1.4f * depth;
                    const float puff  = fbm (fx / (170.0f * sc), fy / (70.0f * sc), 5, 29);
                    const float above = fbm (fx / (170.0f * sc), (fy - 10.0f * sc) / (70.0f * sc), 5, 29);
                    // Oberseiten der Ballen fangen Licht
                    const float shade = juce::jlimit (0.0f, 1.0f, 0.55f + (puff - 0.5f) * 1.1f + (puff - above) * 3.5f - depth * 0.2f);
                    juce::Colour cl = juce::Colour (0xff6d6a96).interpolatedWith (juce::Colour (0xfffde6d6), shade);
                    cl = cl.interpolatedWith (juce::Colour (0xffffc994), 0.35f * juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (fx / W - 0.62f) * 2.5f) * (1.0f - depth));
                    const float edge = juce::jlimit (0.0f, 1.0f, (fy - cloudTop) / 25.0f);
                    c = c.interpolatedWith (cl, edge);
                }
                // naher Grat rechts (Krippenstein)
                if (fy >= peaksNear[(size_t) x])
                    c = juce::Colour (0xff17141f).interpolatedWith (juce::Colour (0xff2c2433), fbm (fx / 20.0f, fy / 20.0f, 3, 41));
                px.set (x, y, c);
            }
        auto img = px.toImage();
        vignette (img, 0.5f);
        return img;
    }

    //==========================================================================
    // SANKT WOLFGANG: gotisches Glasfenster mit Bleiruten, Steinpfosten und Spitzbögen
    inline juce::Image stainedGlass()
    {
        RGB px;
        const juce::Colour palette[] = { juce::Colour (0xff1d3f9e), juce::Colour (0xff9e1b2a), juce::Colour (0xffd9a52a),
                                         juce::Colour (0xff1f7a4a), juce::Colour (0xff5b2a86), juce::Colour (0xff2a6fb8),
                                         juce::Colour (0xffc75a1a), juce::Colour (0xffe8dcb0) };
        const int lancets = 5;
        const float lw = (float) W / lancets, mull = 26.0f, cell = 62.0f;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const int li = juce::jlimit (0, lancets - 1, (int) (fx / lw));
                const float lx = fx - li * lw;                         // Position im Fensterfeld
                const float cx = lw * 0.5f;
                // Spitzbogen oben: zwei Kreisbögen
                const float archTop = 60.0f, archH = lw * 0.9f;
                bool stone = lx < mull * 0.5f || lx > lw - mull * 0.5f;
                if (fy < archTop + archH)
                {
                    const float r = lw - mull;
                    const float dl = std::hypot (lx - (mull * 0.5f), fy - (archTop + archH));
                    const float dr = std::hypot (lx - (lw - mull * 0.5f), fy - (archTop + archH));
                    if (dl > r || dr > r) stone = true;
                }
                if (fy < archTop) stone = true;
                // Querstangen aus Eisen
                const bool bar = std::fmod (fy - archTop, 240.0f) < 7.0f && fy > archTop + archH;

                juce::Colour c;
                if (stone)
                {
                    const float n = fbm (fx / 40.0f, fy / 40.0f, 4, 3);
                    c = juce::Colour (0xff3a352e).interpolatedWith (juce::Colour (0xff6d665a), n);
                    if (std::fmod (fy, 90.0f) < 2.5f) c = c.withMultipliedBrightness (0.6f);   // Steinfugen
                }
                else if (bar) c = juce::Colour (0xff141414);
                else
                {
                    // Glasstücke: Voronoi, Bleiruten an den Kanten
                    const int gx = (int) std::floor (fx / cell), gy = (int) std::floor (fy / cell);
                    float d1 = 1e9f, d2 = 1e9f; int best = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            const float px_ = (gx + dx + 0.15f + 0.7f * hash (gx + dx, gy + dy, 1)) * cell;
                            const float py_ = (gy + dy + 0.15f + 0.7f * hash (gx + dx, gy + dy, 2)) * cell;
                            const float d = std::hypot (fx - px_, fy - py_);
                            if (d < d1) { d2 = d1; d1 = d; best = (int) (hash (gx + dx, gy + dy, 3) * 8.0f); }
                            else if (d < d2) d2 = d;
                        }
                    if (d2 - d1 < 4.0f) c = juce::Colour (0xff121212);
                    else
                    {
                        // Licht fällt von hinten: Mitte heller, Glas mit Schlieren
                        const float glow = 1.0f - 0.55f * juce::jlimit (0.0f, 1.0f, std::hypot ((fx - W * 0.5f) / W, (fy - H * 0.45f) / H) * 1.6f);
                        const float streak = 0.85f + 0.3f * fbm (fx / 12.0f, fy / 70.0f, 3, best + 11);
                        c = palette[juce::jlimit (0, 7, best)].withMultipliedBrightness (glow * streak * (0.9f + 0.25f * (lx > cx ? 0.1f : 0.0f)));
                        if (d2 - d1 < 7.0f) c = c.withMultipliedBrightness (0.75f);
                    }
                }
                px.set (x, y, c);
            }
        auto img = px.toImage();
        {
            juce::Graphics g (img);
            juce::ColourGradient rays (juce::Colour (0x30fff1c8), W * 0.5f, H * 0.35f, juce::Colour (0x00fff1c8), W * 0.5f, H * 1.1f, true);
            g.setGradientFill (rays);
            g.fillAll();
        }
        vignette (img, 0.55f);
        return img;
    }

    //==========================================================================
    // LANDFRIEDALM: Almwiese mit Blumen, Hütte, Bergkulisse, Sommerhimmel
    inline juce::Image almMeadow()
    {
        RGB px;
        const int skyEnd = (int) (H * 0.36f);
        std::vector<float> ridge ((size_t) W), hill ((size_t) W);
        for (int x = 0; x < W; ++x)
        {
            float r = fbm (x / 260.0f, 0.5f, 5, 7); r = 1.0f - std::abs (r * 2.0f - 1.0f);
            ridge[(size_t) x] = skyEnd + 40.0f - 260.0f * r;
            hill[(size_t) x]  = H * 0.47f - 80.0f * fbm (x / 500.0f, 2.0f, 3, 8);
        }
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const float t = fy / skyEnd;
                juce::Colour c = juce::Colour (0xff3d7ccc).interpolatedWith (juce::Colour (0xffb9d9f2), juce::jlimit (0.0f, 1.0f, t));
                const float cl = fbm (fx / 260.0f, fy / 70.0f, 5, 3);
                c = c.interpolatedWith (juce::Colours::white, juce::jlimit (0.0f, 0.9f, (cl - 0.55f) * 3.0f));
                if (fy >= ridge[(size_t) x])       // Felsberge mit Schnee
                {
                    const float sn = juce::jlimit (0.0f, 1.0f, (fbm (fx / 12.0f, fy / 40.0f, 3, 9) - 0.45f) * 3.0f) * juce::jlimit (0.0f, 1.0f, 1.0f - (fy - ridge[(size_t) x]) / 120.0f);
                    c = juce::Colour (0xff7d8597).interpolatedWith (juce::Colour (0xff9aa0ad), fbm (fx / 30.0f, fy / 30.0f, 3, 10)).interpolatedWith (juce::Colours::white, sn);
                    c = c.interpolatedWith (juce::Colour (0xffb9d9f2), 0.25f);                     // Luftperspektive
                }
                if (fy >= hill[(size_t) x])        // Almhänge
                {
                    const float d = (fy - hill[(size_t) x]) / (H - hill[(size_t) x]);
                    c = juce::Colour (0xff5f8f2c).interpolatedWith (juce::Colour (0xff3d6e1d), d + 0.3f * (fbm (fx / 80.0f, fy / 40.0f, 4, 11) - 0.5f));
                    c = c.withMultipliedBrightness (0.9f + 0.25f * fbm (fx / 8.0f, fy / 4.0f, 3, 12));
                }
                px.set (x, y, c);
            }

        juce::Random rng (1873);
        // Almhütte
        {
            const int hx = (int) (W * 0.18f), hy = (int) (hill[(size_t) (W * 0.18f)] + 30);
            for (int y = hy - 70; y < hy; ++y) for (int x = hx; x < hx + 120; ++x)
                px.blend (x, y, 0.38f + 0.06f * ((y / 7) % 2), 0.24f, 0.13f, 0.97f);                  // Holzbalken
            for (int k = 0; k < 50; ++k) for (int x = hx - 14 + k; x < hx + 134 - k; ++x)
                px.blend (x, hy - 70 - k, 0.22f, 0.2f, 0.2f, 0.97f);                                   // Dach
            for (int y = hy - 45; y < hy - 25; ++y) for (int x = hx + 22; x < hx + 40; ++x) px.blend (x, y, 0.95f, 0.9f, 0.7f, 0.9f);
        }
        // Grashalme und Blumen, vorne größer
        for (int i = 0; i < 220000; ++i)
        {
            const int x = rng.nextInt (W);
            const float yy = H * 0.45f + std::pow (rng.nextFloat(), 0.7f) * H * 0.56f;
            const int y = (int) yy;
            if (y < hill[(size_t) x] || y >= H) continue;
            const float persp = (yy - H * 0.45f) / (H * 0.55f);
            const float len = 3.0f + 22.0f * persp * (0.5f + rng.nextFloat());
            const float lean = (rng.nextFloat() - 0.5f) * 0.6f;
            const float g = 0.35f + 0.3f * rng.nextFloat();
            for (float k = 0; k < len; k += 1.0f)
                px.blend ((int) (x + lean * k), (int) (y - k), 0.25f * g + 0.1f, 0.5f * g + 0.25f, 0.12f, 0.7f);
        }
        const float flower[][3] = { { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.86f, 0.2f }, { 0.62f, 0.38f, 0.85f }, { 0.9f, 0.25f, 0.3f }, { 0.35f, 0.55f, 0.95f } };
        for (int i = 0; i < 9000; ++i)
        {
            const int x = rng.nextInt (W);
            const float yy = H * 0.5f + std::pow (rng.nextFloat(), 0.8f) * H * 0.5f;
            if (yy < hill[(size_t) x] + 10) continue;
            const float persp = (yy - H * 0.5f) / (H * 0.5f);
            const int r = 1 + (int) (persp * 5.0f);
            const auto& c = flower[rng.nextInt (5)];
            for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx)
                if (dx * dx + dy * dy <= r * r) px.blend (x + dx, (int) yy + dy, c[0], c[1], c[2], 0.9f);
            px.blend (x, (int) yy, 1.0f, 0.85f, 0.2f, 0.9f);
        }
        auto img = px.toImage();
        vignette (img, 0.45f);
        return img;
    }

    //==========================================================================
    // KELTEN: Bronzeblech mit Patina, getriebene Spiralen (Triskelen), Punktbordüre
    inline juce::Image celticBronze()
    {
        RGB px;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const float n = fbm (fx / 120.0f, fy / 120.0f, 5, 4);
                const float brushed = fbm (fx / 2.0f, fy / 60.0f, 2, 5);
                juce::Colour c = juce::Colour (0xff5a3a1a).interpolatedWith (juce::Colour (0xffc08a3e), n * 0.8f + brushed * 0.25f);
                const float pat = fbm (fx / 70.0f, fy / 70.0f, 5, 6);
                if (pat > 0.55f) c = c.interpolatedWith (juce::Colour (0xff3f8f7a), juce::jlimit (0.0f, 0.85f, (pat - 0.55f) * 4.0f));
                const float rust = fbm (fx / 25.0f, fy / 25.0f, 4, 7);
                if (rust > 0.68f) c = c.interpolatedWith (juce::Colour (0xff6e2d14), juce::jlimit (0.0f, 0.7f, (rust - 0.68f) * 5.0f));
                px.set (x, y, c);
            }
        auto img = px.toImage();
        {
            juce::Graphics g (img);
            // getriebene Triskelen: drei Spiralarme um ein Zentrum; Licht von oben links (hell oben, dunkel unten versetzt)
            auto triskele = [&] (float cx, float cy, float rad, float rot)
            {
                juce::Path p;
                for (int arm = 0; arm < 3; ++arm)
                {
                    const float a0 = rot + arm * juce::MathConstants<float>::twoPi / 3.0f;
                    for (int i = 0; i <= 160; ++i)
                    {
                        const float t = i / 160.0f;
                        const float r = rad * (0.08f + 0.92f * t);
                        const float a = a0 + t * juce::MathConstants<float>::twoPi * 1.6f;
                        const float x = cx + std::cos (a) * r * 0.55f + std::cos (a0) * rad * 0.45f * t;
                        const float y = cy + std::sin (a) * r * 0.55f + std::sin (a0) * rad * 0.45f * t;
                        i == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
                    }
                }
                g.setColour (juce::Colour (0x90000000));
                g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (3, 4));
                g.setColour (juce::Colour (0xc0f0c070));
                g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0x60fff2c0));
                g.strokePath (p, juce::PathStrokeType (1.5f), juce::AffineTransform::translation (-1, -1.5f));
            };
            triskele (W * 0.22f, H * 0.32f, 250.0f, 0.3f);
            triskele (W * 0.72f, H * 0.62f, 320.0f, 1.4f);
            triskele (W * 0.47f, H * 0.9f, 160.0f, 2.2f);
            triskele (W * 0.9f, H * 0.15f, 140.0f, 0.9f);
            // Punktbordüre
            for (float x = 40; x < W - 30; x += 34)
                for (float y : { 34.0f, (float) H - 34.0f })
                {
                    g.setColour (juce::Colour (0x90000000)); g.fillEllipse (x + 2, y + 3, 12, 12);
                    g.setColour (juce::Colour (0xd0e8b868)); g.fillEllipse (x, y, 12, 12);
                }
        }
        vignette (img, 0.6f);
        return img;
    }

    //==========================================================================
    // RÖMER: Bodenmosaik aus Steinchen – Mäanderbordüre und Rosetten
    inline juce::Image romanMosaic()
    {
        juce::Image img (juce::Image::RGB, W, H, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff4a443c));                       // Mörtel
        const float t = 15.0f;                                       // Steinchengröße
        const int tw = (int) (W / t) + 1, th = (int) (H / t) + 1;
        const juce::Colour cream (0xffe6dcc4), terra (0xffa4442a), black (0xff23201c), ochre (0xffc99a3c), slate (0xff5f6f7a);
        // Mäander-Motiv (10 x 10 Steinchen), 1 = dunkel
        static const char* meander[10] = { "1111111111", "1000000001", "1011111101", "1010000101", "1010110101",
                                           "1010100101", "1010111101", "1010000001", "1011111111", "0000000000" };
        juce::Random rng (79);
        for (int j = 0; j < th; ++j)
            for (int i = 0; i < tw; ++i)
            {
                juce::Colour c = cream;
                const int border = 11;
                const bool inBorder = i < border || j < border || i >= tw - border || j >= th - border;
                if (inBorder)
                {
                    const int ii = (j < border || j >= th - border) ? i : j;     // entlang der Kante laufen
                    const int jj = (j < border) ? j : (j >= th - border ? th - 1 - j : (i < border ? i : tw - 1 - i));
                    if (jj == 0 || jj == border - 1) c = black;
                    else c = meander[juce::jlimit (0, 9, jj - 1)][ii % 10] == '1' ? black : cream;
                }
                else
                {
                    // Rosetten auf einem Raster
                    const float sp = 22.0f;
                    const float cx = std::round ((i - border) / sp) * sp + border, cy = std::round ((j - border) / sp) * sp + border;
                    const float d = std::hypot (i - cx, j - cy);
                    const float ang = std::atan2 (j - cy, i - cx);
                    if (d < 2.2f) c = ochre;
                    else if (d < 5.5f) c = (std::fmod (ang + 7.0f, 0.785f) < 0.39f) ? terra : ochre;
                    else if (d < 7.0f) c = black;
                    else if (d < 8.2f) c = slate;
                    // diagonale Flechtbänder zwischen den Rosetten
                    const int di = (i - border) % 22, dj = (j - border) % 22;
                    if (d > 9.0f && (std::abs (di - dj) < 1 || std::abs (di + dj - 22) < 1)) c = terra;
                }
                // jedes Steinchen leicht anders
                const float v = 0.85f + 0.3f * rng.nextFloat();
                c = c.withMultipliedBrightness (v).withRotatedHue ((rng.nextFloat() - 0.5f) * 0.02f);
                const float jx = (rng.nextFloat() - 0.5f) * 2.5f, jy = (rng.nextFloat() - 0.5f) * 2.5f;
                const float inset = 1.6f + rng.nextFloat() * 1.2f;
                auto r = juce::Rectangle<float> (i * t + inset + jx, j * t + inset + jy, t - 2 * inset, t - 2 * inset);
                g.setColour (c);
                g.fillRoundedRectangle (r, 2.0f);
                g.setColour (juce::Colour (0x18ffffff));
                g.drawLine (r.getX() + 1, r.getY() + 1, r.getRight() - 1, r.getY() + 1, 1.0f);
            }
        juce::ColourGradient wear (juce::Colour (0x00000000), W * 0.5f, H * 0.5f, juce::Colour (0x50000000), 0, 0, true);
        g.setGradientFill (wear);
        g.fillAll();
        vignette (img, 0.45f);
        return img;
    }

    //==========================================================================
    // GOISERER PASCHER: Hirschleder mit Ziernaht und grüner Stickerei
    inline juce::Image leather()
    {
        RGB px;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const float n = fbm (fx / 150.0f, fy / 150.0f, 5, 3);
                // Narbung: kleine Zellen
                const float cellN = fbm (fx / 4.0f, fy / 4.0f, 2, 4);
                const float crease = std::abs (fbm (fx / 90.0f, fy / 260.0f, 4, 5) - 0.5f);
                juce::Colour c = juce::Colour (0xff3b2414).interpolatedWith (juce::Colour (0xff8a5a32), n * 0.9f + cellN * 0.2f);
                if (crease < 0.02f) c = c.withMultipliedBrightness (0.7f + 15.0f * crease);
                // abgegriffene, hellere Stellen
                c = c.interpolatedWith (juce::Colour (0xffa87a4c), juce::jlimit (0.0f, 0.4f, (fbm (fx / 300.0f, fy / 300.0f, 3, 6) - 0.6f) * 2.0f));
                px.set (x, y, c);
            }
        auto img = px.toImage();
        {
            juce::Graphics g (img);
            // Ziernaht (doppelt) rundherum
            for (float inset : { 46.0f, 62.0f })
            {
                juce::Path seam;
                seam.addRoundedRectangle (inset, inset, W - 2 * inset, H - 2 * inset, 30.0f);
                juce::Path dashed;
                const float dashes[] = { 12.0f, 8.0f };
                juce::PathStrokeType (3.0f).createDashedStroke (dashed, seam, dashes, 2);
                g.setColour (juce::Colour (0x80000000)); g.fillPath (dashed, juce::AffineTransform::translation (1.5f, 2.0f));
                g.setColour (juce::Colour (0xffe8d6a8)); g.fillPath (dashed);
            }
            // grüne Stickerei: Ranken mit Blättern in den Ecken
            auto vine = [&] (float cx, float cy, float s, float rot)
            {
                juce::Path stem;
                for (int i = 0; i <= 60; ++i)
                {
                    const float t = i / 60.0f;
                    const float a = rot + t * 4.0f;
                    const float r = s * (1.0f - t * 0.7f);
                    const float x = cx + std::cos (a) * r * t * 1.2f, y = cy + std::sin (a) * r * t * 1.2f;
                    i == 0 ? stem.startNewSubPath (x, y) : stem.lineTo (x, y);
                    if (i % 12 == 6)
                    {
                        juce::Path leaf;
                        leaf.addEllipse (-12, -4, 24, 8);
                        g.setColour (juce::Colour (0xff3f7a3a));
                        g.fillPath (leaf, juce::AffineTransform::rotation (a + 0.8f).translated (x, y));
                    }
                }
                g.setColour (juce::Colour (0xff2f6a2c));
                g.strokePath (stem, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0xffeef0e6));                   // kleine weiße Blüte (Edelweiß-artig)
                for (int k = 0; k < 8; ++k)
                {
                    juce::Path petal; petal.addEllipse (-3, -11, 6, 12);
                    g.fillPath (petal, juce::AffineTransform::rotation (k * juce::MathConstants<float>::pi / 4.0f).translated (cx, cy));
                }
                g.setColour (juce::Colour (0xffd8c060)); g.fillEllipse (cx - 4, cy - 4, 8, 8);
            };
            vine (150, 150, 140, 0.0f);
            vine ((float) W - 150, 150, 140, 1.6f);
            vine (150, (float) H - 150, 140, -1.6f);
            vine ((float) W - 150, (float) H - 150, 140, 3.1f);
        }
        vignette (img, 0.55f);
        return img;
    }

    //==========================================================================
    // Alle Landschaften der Serie – in jedem Plugin wählbar

    // HAMMERSCHMIED: gehämmertes, dunkles Eisen mit Hammerschlag-Dellen, Glutrissen und Funken
    inline juce::Image forgedIron()
    {
        RGB px;
        juce::Random rnd (1871);
        // Hammerschläge: zufällig verteilte, runde Dellen
        struct Dent { float x, y, r; };
        std::vector<Dent> dents;
        for (int i = 0; i < 900; ++i) dents.push_back ({ rnd.nextFloat() * W, rnd.nextFloat() * H, 18.0f + rnd.nextFloat() * 46.0f });
        std::vector<float> shade ((size_t) (W * H), 0.0f);
        for (auto& d : dents)
        {
            const int x0 = juce::jmax (0, (int) (d.x - d.r)), x1 = juce::jmin (W - 1, (int) (d.x + d.r));
            const int y0 = juce::jmax (0, (int) (d.y - d.r)), y1 = juce::jmin (H - 1, (int) (d.y + d.r));
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                {
                    const float dx = (x - d.x) / d.r, dy = (y - d.y) / d.r;
                    const float q = dx * dx + dy * dy;
                    if (q < 1.0f) shade[(size_t) (y * W + x)] += (dx * -0.6f + dy * -0.8f) * (1.0f - q) * 0.5f;   // Licht von oben links
                }
        }
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x, fy = (float) y;
                const float n = fbm (fx / 220.0f, fy / 220.0f, 5, 11);
                const float scale = fbm (fx / 18.0f, fy / 18.0f, 3, 12);           // Zunder
                const float sh = juce::jlimit (-0.5f, 0.5f, shade[(size_t) (y * W + x)]);
                juce::Colour c = juce::Colour (0xff121314).interpolatedWith (juce::Colour (0xff4a4d52), juce::jlimit (0.0f, 1.0f, n * 0.8f + scale * 0.25f + sh));
                // bläulich-violette Anlauffarben
                const float temper = fbm (fx / 400.0f, fy / 300.0f, 3, 13);
                if (temper > 0.6f) c = c.interpolatedWith (juce::Colour (0xff3a3f6a), (temper - 0.6f) * 1.5f);
                // Glut: Risse von unten heraus glühend
                const float crack = std::abs (fbm (fx / 120.0f, fy / 70.0f, 5, 14) - 0.5f);
                const float heat = juce::jlimit (0.0f, 1.0f, (fy / H - 0.45f) * 1.8f);
                if (crack < 0.035f * heat)
                {
                    const float k = 1.0f - crack / (0.035f * heat + 1.0e-6f);
                    c = c.interpolatedWith (juce::Colour (0xffff6a10).interpolatedWith (juce::Colour (0xffffd070), k * k), juce::jlimit (0.0f, 1.0f, k * 1.4f));
                }
                // warmer Schein von der Esse unten
                c = c.interpolatedWith (juce::Colour (0xff6a1c04), heat * heat * 0.35f);
                px.set (x, y, c);
            }
        auto img = px.toImage();
        {
            juce::Graphics g (img);
            // Funken: kurze, glühende Bahnen nach oben
            for (int i = 0; i < 260; ++i)
            {
                const float x = rnd.nextFloat() * W, y = H * (0.25f + 0.75f * rnd.nextFloat());
                const float len = 6.0f + rnd.nextFloat() * 34.0f, ang = -1.57f + (rnd.nextFloat() - 0.5f) * 1.6f;
                const float a = 0.35f + 0.65f * rnd.nextFloat();
                juce::Path p; p.startNewSubPath (x, y); p.lineTo (x + std::cos (ang) * len, y + std::sin (ang) * len);
                g.setColour (juce::Colour (0xffff8a20).withAlpha (a * 0.35f));
                g.strokePath (p, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0xffffe2a0).withAlpha (a));
                g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
        return img;
    }

    struct Landscape { const char* key; const char* label; std::function<juce::Image()> make; };
    inline const std::vector<Landscape>& landscapes()
    {
        static const std::vector<Landscape> l = {
            { "moos",        "Moos (MOOS)",                              [] { return mossCushions(); } },
            { "hallstatt",   "Hallst\xc3\xa4ttersee am Abend (HALLSTATT)", [] { return lakeAtDusk(); } },
            { "soleleitung", "Holzrohr mit Salz (SOLELEITUNG)",          [] { return woodPipe(); } },
            { "dachstein",   "Gletscher (DACHSTEIN)",                    [] { return glacierIce(); } },
            { "sarstein",    "Kalkwand (SARSTEIN)",                      [] { return limestone(); } },
            { "5fingers",    "Wolkenmeer vom Krippenstein (5 FINGERS)",  [] { return cloudSea(); } },
            { "sanktwolfgang", "Glasfenster (SANKT WOLFGANG)",           [] { return stainedGlass(); } },
            { "landfriedalm",  "Almwiese (LANDFRIEDALM)",                [] { return almMeadow(); } },
            { "kelten",        "Keltische Bronze (KELTEN)",              [] { return celticBronze(); } },
            { "roemer",        "R\xc3\xb6misches Mosaik (R\xc3\x96MER)",  [] { return romanMosaic(); } },
            { "pascher",       "Hirschleder (GOISERER PASCHER)",         [] { return leather(); } },
            { "hammerschmied", "Geschmiedetes Eisen (HAMMERSCHMIED)",   [] { return forgedIron(); } },
        };
        return l;
    }
    inline std::function<juce::Image()> landscapeFor (const juce::String& key)
    {
        for (auto& l : landscapes()) if (key == l.key) return l.make;
        return landscapes().front().make;
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

    // Hintergrund-Einstellungen (gelten für alle Instanzen eines Plugins)
    //   background_<plugin>: ""  = eigene Landschaft des Plugins
    //                        "tex:<key>" = andere Landschaft der Serie
    //                        sonst Pfad zu einem eigenen Foto
    //   durchsicht: 0 = wenig, 1 = mittel, 2 = viel (wie stark der Hintergrund durch die Felder scheint)
    inline juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "trockenmoos";
        o.folderName          = "trockenmoos";
        o.filenameSuffix      = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
    inline juce::String getSetting (const juce::String& key)
    {
        juce::PropertiesFile f (settingsOptions());
        return f.getValue (key);
    }
    inline void setSetting (const juce::String& key, const juce::String& value)
    {
        juce::PropertiesFile f (settingsOptions());
        if (value.isEmpty()) f.removeValue (key); else f.setValue (key, value);
        f.saveIfNeeded();
    }
    inline juce::String getBackground (const juce::String& plugin)                     { return getSetting ("background_" + plugin); }
    inline void setBackground (const juce::String& plugin, const juce::String& value) { setSetting ("background_" + plugin, value); }
    inline int getDurchsicht()             { return juce::jlimit (0, 2, getSetting ("durchsicht").getIntValue()); }
    inline void setDurchsicht (int v)      { setSetting ("durchsicht", juce::String (v)); }
    inline float glassFactor (int d)       { return d == 0 ? 1.15f : (d == 1 ? 0.8f : 0.45f); }

    // Baut das Hintergrund-Menü (gemeinsam für alle Plugins)
    inline void showBackgroundMenu (juce::Component& target, const juce::String& plugin, const juce::String& ownKey,
                                    std::function<void()> changed, std::unique_ptr<juce::FileChooser>& chooser)
    {
        const auto current = getBackground (plugin);
        juce::PopupMenu m;
        m.addSectionHeader ("Landschaft");
        int id = 1;
        for (auto& l : landscapes())
        {
            const juce::String key (l.key);
            const bool ticked = (current.isEmpty() && key == ownKey) || current == "tex:" + key;
            m.addItem (id++, juce::String::fromUTF8 (l.label), true, ticked);
        }
        m.addSeparator();
        m.addItem (100, "Eigenes Foto laden ...", true, current.isNotEmpty() && ! current.startsWith ("tex:"));
        m.addSeparator();
        m.addSectionHeader ("Durchsicht der Felder");
        const int d = getDurchsicht();
        m.addItem (200, "wenig", true, d == 0);
        m.addItem (201, "mittel", true, d == 1);
        m.addItem (202, "viel", true, d == 2);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                         [plugin, ownKey, changed, &chooser] (int r)
        {
            if (r >= 1 && r <= (int) landscapes().size())
            {
                const juce::String key (landscapes()[(size_t) r - 1].key);
                setBackground (plugin, key == ownKey ? juce::String() : "tex:" + key);
                changed();
            }
            else if (r >= 200 && r <= 202) { setDurchsicht (r - 200); changed(); }
            else if (r == 100)
            {
                chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Hintergrundfoto w\xc3\xa4hlen"),
                    juce::File::getSpecialLocation (juce::File::userPicturesDirectory), "*.jpg;*.jpeg;*.png");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [plugin, changed] (const juce::FileChooser& fc)
                                      {
                                          const auto f = fc.getResult();
                                          if (! f.existsAsFile()) return;
                                          setBackground (plugin, f.getFullPathName());
                                          changed();
                                      });
            }
        });
    }
}

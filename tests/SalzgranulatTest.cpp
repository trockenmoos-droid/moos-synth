#include "TestUtil.h"
#include "../plugins/salzgranulat/Source/PluginProcessor.h"
#include "../plugins/salzgranulat/Source/PluginEditor.h"

static void setP (SalzProcessor& p, const char* id, float v)
{
    auto* a = p.apvts.getParameter (id);
    a->setValueNotifyingHost (a->convertTo0to1 (v));
}

static juce::AudioBuffer<float> sine (double sr, double hz, double sec, double activeSec, float amp = 0.4f)
{
    juce::AudioBuffer<float> b (2, (int) (sr * sec)); b.clear();
    for (int i = 0; i < (int) (sr * activeSec) && i < b.getNumSamples(); ++i)
    {
        const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sr);
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    return b;
}

// Energie bei einer Frequenz (Goertzel), Kanal 0
static double energyAt (const juce::AudioBuffer<float>& b, int start, int num, double hz, double sr)
{
    const double w = juce::MathConstants<double>::twoPi * hz / sr, cw = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < num; ++i) { const double s0 = b.getSample (0, start + i) + cw * s1 - s2; s2 = s1; s1 = s0; }
    return (s1 * s1 + s2 * s2 - cw * s1 * s2) / num;
}

static int zeroCrossings (const juce::AudioBuffer<float>& b, int start, int num)
{
    int z = 0;
    for (int i = start + 1; i < start + num; ++i) if ((b.getSample (0, i - 1) < 0.0f) != (b.getSample (0, i) < 0.0f)) ++z;
    return z;
}

static void neutral (SalzProcessor& p)
{
    p.setCurrentProgram (0);
    for (auto [id, v] : std::vector<std::pair<const char*, float>> {
            { "klang", 0.0f }, { "verdunstung", 0.0f }, { "rueckwaerts", 0.0f }, { "mix", 1.0f }, { "anschlag", 0.0f },
            { "rekristall", 0.0f }, { "streuung", 0.0f }, { "korn", 200.0f }, { "dichte", 16.0f }, { "tiefe", 200.0f } })
        setP (p, id, v);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    SalzProcessor proc;

    // Gitter-Mathematik
    tt::check (salz::snap (5.2f, 2) == 4.0f && salz::snap (-4.4f, 2) == -5.0f && salz::snap (13.0f, 0) == 12.0f,
               "Gitter: 5.2 -> 4 (Dur), -4.4 -> -5 (Dur), 13 -> 12 (Oktaven)");

    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto pl = tt::plucks (sr, 10.0, 4.0);
        auto out = tt::run (proc, pl, sr);
        const float pk = tt::peak (out, 0, out.getNumSamples());
        const float act = tt::peak (out, (int) sr, (int) (3 * sr));
        tt::check (tt::finite (out) && pk < 1.2f && act > 0.02f,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Spitze=%.3f  aktiv=%.3f", pk, act));
    }

    // Kristall rastet die Tonhöhe ein: Sinus 440 Hz, Gitter Oktaven, Lage ±12
    auto offRatio = [&] (float k)
    {
        neutral (proc);
        setP (proc, "kristall", k); setP (proc, "gitter", 0.0f); setP (proc, "lage", 12.0f);
        auto out = tt::run (proc, sine (sr, 440.0, 6.0, 6.0), sr);
        const int s0 = (int) (1.5 * sr), num = (int) (4.0 * sr);
        double on = 0, off = 0;
        for (double hz : { 220.0, 440.0, 880.0 }) on += energyAt (out, s0, num, hz, sr);
        for (double hz : { 293.7, 349.2, 554.4, 659.3, 740.0 }) off += energyAt (out, s0, num, hz, sr);
        return on / juce::jmax (1.0e-12, off);
    };
    {
        const double crystal = offRatio (1.0f), brine = offRatio (0.0f);
        tt::check (crystal > 30.0 && crystal > brine * 5.0,
                   juce::String::formatted ("Kristall rastet Tonhoehen ein: Oktav-/Fremdenergie Kristall=%.0f  Sole=%.1f", crystal, brine));
    }

    // Verdunstung: Körner gleiten nach oben
    {
        neutral (proc);
        setP (proc, "kristall", 0.0f); setP (proc, "lage", 0.0f); setP (proc, "korn", 400.0f);
        auto flat = tt::run (proc, sine (sr, 440.0, 5.0, 5.0), sr);
        setP (proc, "verdunstung", 1.0f);
        auto rise = tt::run (proc, sine (sr, 440.0, 5.0, 5.0), sr);
        const int z0 = zeroCrossings (flat, (int) sr, (int) (3 * sr)), z1 = zeroCrossings (rise, (int) sr, (int) (3 * sr));
        tt::check (z1 > z0 * 1.2, juce::String::formatted ("Verdunstung hebt die Tonhoehe: Nulldurchgaenge %d -> %d", z0, z1));
    }

    // Salzkruste: das Lager friert ein und klingt weiter, wenn der Eingang verstummt
    {
        neutral (proc);
        setP (proc, "kristall", 0.3f); setP (proc, "tiefe", 1000.0f);
        auto in = sine (sr, 330.0, 8.0, 3.0);
        auto out = tt::run (proc, in, sr, [&] (int pos, juce::MidiBuffer&)
        {
            setP (proc, "kruste", pos >= (int) (2.5 * sr) ? 1.0f : 0.0f);
        });
        const float held = out.getRMSLevel (0, (int) (5 * sr), (int) (2.5 * sr));
        setP (proc, "kruste", 0.0f);
        auto out2 = tt::run (proc, in, sr);
        const float gone = out2.getRMSLevel (0, (int) (5 * sr), (int) (2.5 * sr));
        tt::check (held > 0.02f && gone < held * 0.1f, juce::String::formatted ("Salzkruste haelt den Klang: mit=%.3f  ohne=%.4f", held, gone));
    }

    // Anschlag: Transienten werfen Körner
    {
        juce::AudioBuffer<float> clicks (2, (int) (sr * 4)); clicks.clear();
        for (int c = 0; c < 8; ++c)
            for (int i = 0; i < 400; ++i)
            {
                const float v = 0.7f * std::sin (i * 0.3f) * (1.0f - i / 400.0f);
                clicks.setSample (0, c * (int) (0.5 * sr) + i, v); clicks.setSample (1, c * (int) (0.5 * sr) + i, v);
            }
        neutral (proc); setP (proc, "dichte", 1.0f); setP (proc, "kristall", 0.0f); setP (proc, "korn", 60.0f); setP (proc, "tiefe", 0.0f);
        auto quiet = tt::run (proc, clicks, sr);
        setP (proc, "anschlag", 1.0f);
        auto hit = tt::run (proc, clicks, sr);
        const float a = quiet.getRMSLevel (0, 0, quiet.getNumSamples()), b = hit.getRMSLevel (0, 0, hit.getNumSamples());
        tt::check (b > a * 2.0f && b > 0.01f, juce::String::formatted ("Anschlag: RMS ohne=%.4f  mit=%.4f", a, b));
    }

    // Hörprobe: Plucks durch fünf Presets
    {
        auto pl = tt::plucks (sr, 7.0, 4.0);
        const int seg = pl.getNumSamples();
        const int sel[] = { 0, 2, 3, 5, 7 };
        juce::AudioBuffer<float> demo (2, seg * 5);
        for (int j = 0; j < 5; ++j)
        {
            proc.setCurrentProgram (sel[j]);
            auto out = tt::run (proc, pl, sr);
            for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, j * seg, out, ch, 0, seg);
        }
        tt::writeWav (dir.getChildFile ("salzgranulat_demo.wav"), demo, sr);
    }

    proc.setCurrentProgram (2);
    auto pl = tt::plucks (sr, 4.0, 4.0);
    tt::run (proc, pl, sr);
    tt::waitForTexture ("salzgranulat", tmk::tex::landscapeFor ("salzgranulat"), dir);
    tt::screenshots<SalzEditor> (proc, dir, "salzgranulat", 1);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}

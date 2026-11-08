#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace salz;

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using PB = juce::AudioParameterBool;
    using PI = juce::AudioParameterInt; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    // Körner
    p.push_back (std::make_unique<P>  (ID { "korn", 1 },        u8 ("Korngr\xc3\xb6\xc3\x9f" "e (ms)"), tmk::skewed (5.0f, 800.0f, 80.0f), 90.0f));
    p.push_back (std::make_unique<P>  (ID { "dichte", 1 },      u8 ("Dichte (K\xc3\xb6rner/s)"), tmk::skewed (1.0f, 120.0f, 18.0f), 16.0f));
    p.push_back (std::make_unique<P>  (ID { "tiefe", 1 },       "Tiefe (ms)", tmk::skewed (0.0f, 8000.0f, 1000.0f), 250.0f));
    p.push_back (std::make_unique<P>  (ID { "streuung", 1 },    "Streuung", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "rueckwaerts", 1 }, u8 ("R\xc3\xbc" "ckw\xc3\xa4rts"), 0.0f, 1.0f, 0.15f));
    // Kristall
    p.push_back (std::make_unique<P>  (ID { "kristall", 1 },    "Kristall", 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<PC> (ID { "gitter", 1 },      "Gitter", lattices, 2));
    p.push_back (std::make_unique<PC> (ID { "grundton", 1 },    "Grundton", keys, 9));
    p.push_back (std::make_unique<PI> (ID { "lage", 1 },        u8 ("Lage (\xc2\xb1HT)"), 0, 24, 12));
    p.push_back (std::make_unique<P>  (ID { "klang", 1 },       "Klang", 0.0f, 1.0f, 0.3f));
    // Verwandlung
    p.push_back (std::make_unique<P>  (ID { "verdunstung", 1 }, "Verdunstung", -1.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "rekristall", 1 },  "Rekristallisation", 0.0f, 0.95f, 0.2f));
    p.push_back (std::make_unique<P>  (ID { "anschlag", 1 },    "Anschlag", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<PB> (ID { "kruste", 1 },      "Salzkruste (Freeze)", false));
    // Ausgang
    p.push_back (std::make_unique<P>  (ID { "breite", 1 },      "Breite", 0.0f, 1.0f, 0.8f));
    p.push_back (std::make_unique<P>  (ID { "mix", 1 },         "Mix", 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "ausgang", 1 },     "Ausgang (dB)", -24.0f, 12.0f, 0.0f));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Steinsalz",            {} },
        { "Sole-Nebel",           { { "kristall", 0.0f }, { "dichte", 60.0f }, { "korn", 180.0f }, { "streuung", 0.8f }, { "lage", 7 },
                                    { "klang", 0.1f }, { "rekristall", 0.35f }, { "mix", 0.6f }, { "anschlag", 0.0f } } },
        { "Kristallgitter",       { { "kristall", 1.0f }, { "dichte", 8.0f }, { "korn", 60.0f }, { "tiefe", 500.0f }, { "streuung", 0.2f },
                                    { "klang", 0.55f }, { "gitter", 3 }, { "rueckwaerts", 0.25f }, { "anschlag", 0.4f }, { "mix", 0.55f } } },
        { "Verdunstung",          { { "verdunstung", 0.8f }, { "kristall", 0.3f }, { "korn", 350.0f }, { "dichte", 14.0f }, { "lage", 5 },
                                    { "rekristall", 0.5f }, { "klang", 0.2f }, { "mix", 0.6f } } },
        { "Salzkruste",           { { "kristall", 0.15f }, { "korn", 400.0f }, { "dichte", 30.0f }, { "tiefe", 1500.0f },
                                    { "streuung", 0.6f }, { "rekristall", 0.0f }, { "mix", 0.8f }, { "anschlag", 0.0f } } },
        { "Bergkristall-Quinten", { { "gitter", 1 }, { "kristall", 0.85f }, { "klang", 0.85f }, { "korn", 40.0f }, { "dichte", 12.0f },
                                    { "lage", 19 }, { "grundton", 2 }, { "mix", 0.5f } } },
        { "Salzstock-Echo",       { { "kristall", 0.9f }, { "tiefe", 3000.0f }, { "streuung", 0.7f }, { "korn", 220.0f }, { "dichte", 4.0f },
                                    { "lage", 0 }, { "klang", 0.0f }, { "rekristall", 0.4f }, { "anschlag", 0.0f }, { "mix", 0.45f } } },
        { "Fleur de Sel",         { { "gitter", 4 }, { "kristall", 0.7f }, { "lage", 24 }, { "korn", 25.0f }, { "dichte", 6.0f },
                                    { "klang", 0.65f }, { "verdunstung", 0.25f }, { "breite", 1.0f }, { "mix", 0.4f } } },
        { "Salzsturm",            { { "kristall", 0.2f }, { "anschlag", 1.0f }, { "dichte", 45.0f }, { "korn", 30.0f }, { "streuung", 1.0f },
                                    { "rueckwaerts", 0.5f }, { "gitter", 6 }, { "lage", 24 }, { "verdunstung", -0.5f }, { "rekristall", 0.3f } } },
    };
}

//==============================================================================
SalzProcessor::SalzProcessor()
    : tmk::Processor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "SALZGRANULAT", factoryPresets())
{
    auto get = [this] (const char* i) { return apvts.getRawParameterValue (i); };
    korn = get ("korn"); dichte = get ("dichte"); tiefe = get ("tiefe"); streuung = get ("streuung"); rueckwaerts = get ("rueckwaerts");
    kristall = get ("kristall"); gitter = get ("gitter"); grundton = get ("grundton"); lage = get ("lage"); klang = get ("klang");
    verdunstung = get ("verdunstung"); rekristall = get ("rekristall"); anschlag = get ("anschlag"); kruste = get ("kruste");
    breite = get ("breite"); mix = get ("mix"); ausgang = get ("ausgang");
    combs.assign ((size_t) (maxGrains * combLen), 0.0f);
}

bool SalzProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (l.getMainInputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainInputChannelSet() == juce::AudioChannelSet::mono());
}

void SalzProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    lagerSize = (int) (sr * 12.0) + 16;
    for (auto& l : lager) l.assign ((size_t) lagerSize, 0.0f);
    w = 0;
    for (auto& g : grains) g.active = false;
    std::fill (combs.begin(), combs.end(), 0.0f);
    untilGrain = 0.0; sampleClock = 0; lastGridSlot = -1.0;
    envFast = envSlow = 0.0f; holdOff = 0; silentSamples = 0;
    for (int c = 0; c < 2; ++c) fbHp[c] = fbHpX[c] = lastWet[c] = 0.0f;
    mixSm.reset (sr, 0.05); mixSm.setCurrentAndTargetValue (mix->load());
    outSm.reset (sr, 0.05); outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (ausgang->load()));
}

float SalzProcessor::read (int ch, double pos) const noexcept
{
    while (pos < 0.0) pos += lagerSize;
    while (pos >= lagerSize) pos -= lagerSize;
    const int i0 = juce::jlimit (0, lagerSize - 1, (int) pos), i1 = i0 + 1 >= lagerSize ? 0 : i0 + 1;
    const float f = (float) (pos - i0);
    const auto& b = lager[ch];
    return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
}

//==============================================================================
void SalzProcessor::spawn (bool burst)
{
    int slot = -1;
    for (int i = 0; i < maxGrains; ++i) if (! grains[i].active) { slot = i; break; }
    if (slot < 0) return;
    auto& g = grains[slot];

    const float k = kristall->load();
    const double beatSamples = 60.0 / juce::jmax (20.0, readBpm()) * sr;
    const bool frozen = kruste->load() > 0.5f;

    // Länge
    int len = (int) (korn->load() * 0.001 * sr * (burst ? 0.7 : 1.0) * (1.0 + (rng.nextDouble() - 0.5) * 0.4 * (1.0 - k)));
    len = juce::jlimit (64, (int) (sr * 1.2), len);

    // Tonhöhe: Sole = frei im Bereich ±Lage, Kristall = auf den nächsten Gitterton gezogen
    const int lat = (int) gitter->load();
    const float range = (float) lage->load();
    const float r = (rng.nextFloat() * 2.0f - 1.0f) * range;
    const float semis = r + (snap (r, lat) - r) * k;
    double rate = std::pow (2.0, semis / 12.0);
    const double v = verdunstung->load();                         // ±1 Oktave über die Kornlänge
    const double glideTotal = std::pow (2.0, v);
    const bool reverse = rng.nextFloat() < rueckwaerts->load();
    const double maxRate = rate * juce::jmax (1.0, glideTotal);

    // Leseposition: Tiefe + Streuung; Kristall rastet auf ganze Schläge ein
    double back = tiefe->load() * 0.001 * sr;
    back += streuung->load() * rng.nextDouble() * (0.25 * back + 1.5 * sr);
    if (back > beatSamples * 0.5)
    {
        const double q = juce::jmax (1.0, std::round (back / beatSamples)) * beatSamples;
        back += (q - back) * k;
    }
    if (burst) back = 0.0;
    if (frozen) back = juce::jmax (back, maxRate * len + 64.0);                // im festen Lager nicht über die Nahtstelle lesen
    else
    {
        // der Lesekopf darf den Schreibkopf nicht überholen und nicht in überschriebene Bereiche laufen
        const double need = reverse ? 64.0 : juce::jmax (0.0, (maxRate - 1.0) * len) + 64.0;
        back = juce::jlimit (need, juce::jmax (need, lagerSize - maxRate * len - 256.0), back + (burst ? 0.02 * sr : 0.0));
    }
    g.pos = (double) w - back;
    if (reverse) { g.pos += len * rate; rate = -rate; }
    g.step = rate;
    g.glide = std::pow (glideTotal, 1.0 / len);
    g.age = 0; g.len = len;

    // Raum: Sole = irgendwo, Kristall = fünf feste Gitterplätze
    const float b = breite->load();
    float pan = rng.nextFloat() * 2.0f - 1.0f;
    const float lattice = std::round (pan * 2.0f) * 0.5f;
    pan = (pan + (lattice - pan) * k) * b;
    g.gl = std::sqrt (0.5f * (1.0f - pan)); g.gr = std::sqrt (0.5f * (1.0f + pan));
    g.amp = burst ? 1.1f : (0.75f + 0.25f * rng.nextFloat());
    g.shape = k;

    // Kristall-Resonator: auf einen Gitterton über dem Grundton gestimmt
    const float kl = klang->load();
    g.combMix = kl;
    g.combFb = kl > 0.001f ? 0.6f + 0.385f * std::sqrt (kl) : 0.0f;
    const double keyHz = 261.63 * std::pow (2.0, grundton->load() / 12.0);
    const float note = snap (semis * 0.5f + (rng.nextFloat() - 0.5f) * 12.0f * (1.0f - k), lat);
    const double hz = juce::jlimit (60.0, 3000.0, keyHz * std::pow (2.0, note / 12.0));
    const double d = juce::jlimit (4.0, (double) combLen - 4.0, sr / hz);
    g.combD = (int) d; g.combFrac = (float) (d - g.combD);
    g.cw = 0; g.lp = 0.0f;
    std::fill_n (combs.begin() + slot * combLen, combLen, 0.0f);

    g.vx = pan; g.vy = semis / 24.0f; g.vz = (float) juce::jlimit (0.0, 1.0, back / (sr * 8.0));
    g.active = true;
}

//==============================================================================
void SalzProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    if (inCh == 0) buffer.clear();                                  // kein Eingang: nicht den eigenen Ausgang lesen
    else if (inCh == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    // Takt für das Kristallraster
    double bpm = 120.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying() && pos->getPpqPosition().hasValue()) ppq = *pos->getPpqPosition() + chunkOffset * (bpm / 60.0 / sr);
        }
    const double bps = bpm / 60.0 / sr;
    if (ppq < 0.0) ppq = (double) sampleClock * bps;

    const float k = kristall->load(), dens = dichte->load(), att = anschlag->load(), fb = rekristall->load();
    const bool frozen = kruste->load() > 0.5f;
    // Raster: die Zweierteilung des Schlags, die der Dichte am nächsten kommt
    const double perBeat = dens * 60.0 / bpm;
    const double gridBeats = std::pow (2.0, juce::jlimit (-4.0, 2.0, std::round (std::log2 (1.0 / juce::jmax (0.01, perBeat)))));
    // Lautstärke-Ausgleich für viele überlappende Körner
    wetGainNorm = 1.0f / std::sqrt (juce::jmax (1.0f, dens * korn->load() * 0.001f * 0.6f));
    mixSm.setTargetValue (mix->load());
    outSm.setTargetValue (juce::Decibels::decibelsToGain (ausgang->load()));
    const float randomRate = dens * (1.0f - k);
    float inPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i], inR = R[i];
        const float mono = 0.5f * (inL + inR);
        inPeak = juce::jmax (inPeak, std::abs (mono));

        // Anschlag: Transienten werfen eine Handvoll Körner
        const float a = std::abs (mono);
        envFast += (a - envFast) * (a > envFast ? 0.3f : 0.002f);
        envSlow += (a - envSlow) * 0.0002f;
        if (holdOff > 0) --holdOff;
        else if (att > 0.01f && envFast > envSlow * 2.5f + 0.02f)
        {
            const int num = 1 + (int) (att * 9.0f);
            for (int j = 0; j < num; ++j) spawn (true);
            holdOff = (int) (0.08 * sr);
        }

        // Salzlager beschreiben (mit Rekristallisation), außer die Kruste hält es fest.
        // Ohne Eingangssignal trocknet die Rückführung nach 3 s langsam aus – so verklingt jede Fahne irgendwann.
        silentSamples = a > 1.0e-4f ? 0 : silentSamples + 1;
        const float dry = silentSamples < (int) (3.0 * sr) ? 1.0f
                        : (float) std::exp (-(silentSamples - 3.0 * sr) / (1.5 * sr));
        if (! frozen)
        {
            for (int c = 0; c < 2; ++c)
            {
                // Hochpass gegen Gleichanteil in der Rückführung
                const float x = lastWet[c];
                fbHp[c] = 0.995f * (fbHp[c] + x - fbHpX[c]); fbHpX[c] = x;
                const float in = c == 0 ? inL : inR;
                lager[c][(size_t) w] = in + std::tanh (fbHp[c] * fb * dry) * 0.9f;
            }
            if (++w >= lagerSize) w = 0;
        }

        // Körner auslösen: Sole = zufällig (Poisson), Kristall = auf dem Raster
        if (randomRate > 0.05f)
        {
            untilGrain -= 1.0;
            if (untilGrain <= 0.0)
            {
                spawn (false);
                untilGrain = -std::log (juce::jmax (1.0e-6, rng.nextDouble())) * sr / randomRate;
            }
        }
        if (k > 0.001f)
        {
            const double slot = std::floor ((ppq + i * bps) / gridBeats);
            if (slot != lastGridSlot)
            {
                lastGridSlot = slot;
                if (rng.nextFloat() < k) spawn (false);
            }
        }

        // Körner spielen
        float wl = 0.0f, wr = 0.0f;
        int active = 0;
        for (int gi = 0; gi < maxGrains; ++gi)
        {
            auto& g = grains[gi];
            if (! g.active) continue;
            ++active;
            float x = 0.0f;
            if (g.age < g.len)
            {
                const float t = (float) g.age / (float) g.len;
                const float hann = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);
                // Kristall-Hüllkurve: scharfe Kante, dann Abklingen (wie ein Salzkorn, das aufschlägt)
                const float atk = juce::jmin (1.0f, (float) g.age / (0.002f * (float) sr));
                const float perc = atk * (1.0f - t) * (1.0f - t) * (1.0f - t) * juce::jmin (1.0f, (1.0f - t) * 40.0f);
                const float env = hann + (perc - hann) * g.shape;
                x = 0.5f * (read (0, g.pos) + read (1, g.pos)) * env * g.amp;
                g.pos += g.step;
                g.step *= g.glide;
            }
            float y = x * (1.0f - 0.6f * g.combMix);
            if (g.combFb > 0.0f)
            {
                float* cb = combs.data() + gi * combLen;
                int ri = g.cw - g.combD; if (ri < 0) ri += combLen;
                int ri2 = ri - 1; if (ri2 < 0) ri2 += combLen;
                const float del = cb[ri] + g.combFrac * (cb[ri2] - cb[ri]);
                g.lp += (del - g.lp) * 0.55f;                                  // Dämpfung im Kristall
                const float c = x * (1.0f - g.combFb) * 3.0f + g.lp * g.combFb;
                cb[g.cw] = c;
                if (++g.cw >= combLen) g.cw = 0;
                float tailFade = 1.0f;
                const int tailMax = (int) (1.5 * sr);
                if (g.age > g.len + tailMax - 256) tailFade = (float) (g.len + tailMax - g.age) / 256.0f;
                y += c * g.combMix * 1.6f * tailFade;
                if (g.age >= g.len && (g.age > g.len + tailMax || (std::abs (c) < 1.0e-5f && g.age > g.len + 2 * combLen)))
                    g.active = false;
            }
            else if (g.age >= g.len) g.active = false;
            ++g.age;
            wl += y * g.gl; wr += y * g.gr;
        }
        activeGrains = active;

        wl *= wetGainNorm * 1.4f; wr *= wetGainNorm * 1.4f;
        lastWet[0] = wl; lastWet[1] = wr;
        const float m = mixSm.getNextValue(), og = outSm.getNextValue();
        // nur der Nassanteil wird weich begrenzt – das trockene Signal bleibt unberührt
        L[i] = (inL * (1.0f - m) + std::tanh (wl * 0.9f) / 0.9f * m) * og;
        R[i] = (inR * (1.0f - m) + std::tanh (wr * 0.9f) / 0.9f * m) * og;
        ++sampleClock;
    }

    // Anzeige
    for (int gi = 0; gi < maxGrains; ++gi)
    {
        auto& g = grains[gi];
        auto& vw = view[gi];
        if (g.active)
        {
            const float t = juce::jmin (1.0f, (float) g.age / (float) g.len);
            vw.x = g.vx; vw.y = g.vy; vw.z = g.vz;
            vw.glow = g.age < g.len ? (0.4f + 0.6f * std::sin (juce::MathConstants<float>::pi * t)) : 0.35f * g.combMix;
        }
        else vw.glow = vw.glow.load() * 0.8f;
    }
    crystalNow = k;
    inputLevel = juce::jmax (inPeak, inputLevel.load() * 0.9f);
}

juce::AudioProcessorEditor* SalzProcessor::createEditor() { return new SalzEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SalzProcessor(); }

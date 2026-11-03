#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <deque>

using namespace hammer;

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using PB = juce::AudioParameterBool;
    using PI = juce::AudioParameterInt; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<PB> (ID { "frei", 1 },        "Frei laufen", true));
    p.push_back (std::make_unique<P>  (ID { "swing", 1 },       "Swing", 0.0f, 0.7f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "chaos", 1 },       "Chaos", 0.0f, 1.0f, 0.25f));
    p.push_back (std::make_unique<P>  (ID { "mutation", 1 },    "Mutation", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "hitze", 1 },       "Hitze", 0.0f, 1.0f, 0.25f));
    p.push_back (std::make_unique<P>  (ID { "volume", 1 },      u8 ("Lautst\xc3\xa4rke (dB)"), -48.0f, 6.0f, -4.0f));
    p.push_back (std::make_unique<P>  (ID { "glut", 1 },        "Glut", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "korn", 1 },        u8 ("Korngr\xc3\xb6\xc3\x9f" "e (ms)"), tmk::skewed (5.0f, 300.0f, 50.0f), 45.0f));
    p.push_back (std::make_unique<P>  (ID { "dichte", 1 },      "Dichte", 0.0f, 1.0f, 0.4f));
    p.push_back (std::make_unique<PI> (ID { "tonhoehe", 1 },    u8 ("Tonh\xc3\xb6he (HT)"), -12, 12, 0));
    p.push_back (std::make_unique<P>  (ID { "rueckwaerts", 1 }, u8 ("R\xc3\xbc" "ckw\xc3\xa4rts"), 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "stotter", 1 },     "Stotter", 0.0f, 1.0f, 0.25f));
    p.push_back (std::make_unique<P>  (ID { "streuung", 1 },    "Streuung", 0.0f, 1.0f, 0.4f));

    // Voreinstellung: ein ungerades, aber treibendes Grundmuster
    const int   defSteps[numTracks] = { 16, 13, 16, 11, 7, 9 };
    const int   defHits [numTracks] = { 5, 3, 4, 7, 3, 4 };
    const int   defRot  [numTracks] = { 0, 2, 4, 1, 3, 5 };
    const float defLvl  [numTracks] = { 0.9f, 0.6f, 0.65f, 0.45f, 0.4f, 0.5f };
    const float defCh   [numTracks] = { 1.0f, 0.85f, 0.9f, 0.8f, 0.7f, 0.75f };
    const int   defRate [numTracks] = { 3, 3, 3, 6, 5, 4 };
    for (int t = 0; t < numTracks; ++t)
    {
        const juce::String n = u8 (trackNames[t]);
        p.push_back (std::make_unique<P>  (ID { pid (t, "lvl"), 1 },    n + " Pegel", 0.0f, 1.0f, defLvl[t]));
        p.push_back (std::make_unique<P>  (ID { pid (t, "ton"), 1 },    n + " Ton", -1.0f, 1.0f, 0.0f));
        p.push_back (std::make_unique<P>  (ID { pid (t, "abk"), 1 },    n + " Abklingen", 0.0f, 1.0f, 0.4f));
        p.push_back (std::make_unique<PI> (ID { pid (t, "sch"), 1 },    n + " Schritte", 2, 32, defSteps[t]));
        p.push_back (std::make_unique<PI> (ID { pid (t, "hit"), 1 },    n + u8 (" Schl\xc3\xa4ge"), 0, 32, defHits[t]));
        p.push_back (std::make_unique<PI> (ID { pid (t, "dreh"), 1 },   n + " Drehung", 0, 31, defRot[t]));
        p.push_back (std::make_unique<P>  (ID { pid (t, "chance"), 1 }, n + " Chance", 0.0f, 1.0f, defCh[t]));
        p.push_back (std::make_unique<PC> (ID { pid (t, "tempo"), 1 },  n + " Tempo", rates, defRate[t]));
    }
    return { p.begin(), p.end() };
}

// Hilfe für Presets: Spurparameter per Name
static const char* T (int track, const char* param)
{
    static std::deque<std::string> keep;
    keep.push_back (pid (track, param).toStdString());
    return keep.back().c_str();
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Hammerschmiede", {} },
        { "Gl\xc3\xbchende Esse", { { T (3, "sch"), 16 }, { T (3, "hit"), 13 }, { T (3, "tempo"), 6 }, { T (3, "lvl"), 0.55f },
                                    { T (4, "hit"), 5 }, { T (4, "lvl"), 0.5f }, { "chaos", 0.4f }, { "glut", 0.2f }, { "hitze", 0.35f } } },
        { "Amboss-Polyrhythmus",   { { T (1, "sch"), 7 }, { T (1, "hit"), 3 }, { T (1, "tempo"), 5 }, { T (1, "lvl"), 0.8f }, { T (1, "abk"), 0.7f },
                                    { T (5, "sch"), 5 }, { T (5, "hit"), 2 }, { T (5, "tempo"), 4 }, { T (0, "hit"), 4 },
                                    { "chaos", 0.1f }, { "mutation", 0.15f }, { "glut", 0.15f } } },
        { "Funkenflug",            { { T (4, "sch"), 16 }, { T (4, "hit"), 9 }, { T (4, "tempo"), 6 }, { T (4, "lvl"), 0.7f }, { T (4, "chance"), 0.6f },
                                    { "chaos", 0.7f }, { "glut", 0.45f }, { "korn", 18.0f }, { "dichte", 0.7f }, { "tonhoehe", 7 } } },
        { "Wasserrad",             { { "chaos", 0.05f }, { "mutation", 0.6f }, { "glut", 0.1f }, { "stotter", 0.05f }, { "swing", 0.25f },
                                    { T (0, "sch"), 16 }, { T (0, "hit"), 4 }, { T (2, "sch"), 16 }, { T (2, "hit"), 2 }, { T (2, "dreh"), 4 } } },
        { "Glitch-Geselle",        { { "stotter", 0.8f }, { "glut", 0.7f }, { "chaos", 0.55f }, { "tonhoehe", 12 }, { "rueckwaerts", 0.5f },
                                    { "korn", 30.0f }, { "dichte", 0.6f }, { "hitze", 0.45f } } },
        { "Kalte Schmiede",        { { T (0, "hit"), 2 }, { T (1, "abk"), 1.0f }, { T (1, "hit"), 2 }, { T (2, "hit"), 1 }, { T (3, "hit"), 3 },
                                    { T (4, "hit"), 1 }, { T (5, "hit"), 2 }, { "chaos", 0.15f }, { "glut", 0.55f }, { "korn", 180.0f },
                                    { "dichte", 0.8f }, { "streuung", 0.9f }, { "stotter", 0.0f }, { "hitze", 0.0f } } },
        { "Hammerwerk 7/8",        { { T (0, "sch"), 14 }, { T (0, "hit"), 5 }, { T (1, "sch"), 14 }, { T (1, "hit"), 3 }, { T (2, "sch"), 14 },
                                    { T (2, "hit"), 4 }, { T (3, "sch"), 14 }, { T (3, "hit"), 9 }, { T (4, "sch"), 14 }, { T (5, "sch"), 14 },
                                    { "chaos", 0.2f }, { "mutation", 0.2f } } },
    };
}

//==============================================================================
HammerProcessor::HammerProcessor()
    : tmk::Processor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "HAMMERSCHMIED", factoryPresets())
{
    auto get = [this] (const juce::String& i) { return apvts.getRawParameterValue (i); };
    frei = get ("frei"); swing = get ("swing"); chaos = get ("chaos"); mutation = get ("mutation"); hitze = get ("hitze"); volume = get ("volume");
    glut = get ("glut"); korn = get ("korn"); dichte = get ("dichte"); tonhoehe = get ("tonhoehe"); rueckwaerts = get ("rueckwaerts");
    stotter = get ("stotter"); streuung = get ("streuung");
    for (int t = 0; t < numTracks; ++t)
    {
        level[t] = get (pid (t, "lvl")); tune[t] = get (pid (t, "ton")); decay[t] = get (pid (t, "abk")); steps[t] = get (pid (t, "sch"));
        hitsP[t] = get (pid (t, "hit")); rotate[t] = get (pid (t, "dreh")); chance[t] = get (pid (t, "chance")); rate[t] = get (pid (t, "tempo"));
    }
    pending.reserve (256);
}

bool HammerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void HammerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& tr : hits) for (auto& h : tr) h.active = false;
    pending.clear();
    freePpq = 0.0; lastBar = -1; lastBaseStep = -1;
    for (int t = 0; t < numTracks; ++t) { rotOff[t] = hitOff[t] = 0; }
    for (auto& r : rec) r.assign ((size_t) (sr * 3.0) + 8, 0.0f);
    recW = 0;
    for (auto& g : grains) g.active = false;
    stut = {}; stutMix = 0.0f; untilGrain = 0.0;
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, maxChunk));
    gainSm.reset (sr, 0.05);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
}

//==============================================================================
void HammerProcessor::trigger (int t, int, float vel)
{
    auto& h = hits[t][nextSlot[t]];
    nextSlot[t] = (nextSlot[t] + 1) % 3;
    const float tn = tune[t]->load() + (chaos->load() * 0.15f) * (rng.nextFloat() - 0.5f);   // Chaos verstimmt leicht
    const float dc = decay[t]->load();
    h = Hit();
    h.active = true; h.type = t; h.vel = vel; h.age = 0;
    auto envFor = [this] (float sec) { return (float) std::exp (-6.9 / (juce::jmax (0.002f, sec) * sr)); };

    switch (t)
    {
        case 0:   // Hammer: tiefer Schlag mit Tonhöhen-Abfall und Klick
            h.baseHz = 46.0 * std::pow (2.0, tn);
            h.envCoef = envFor (0.12f + dc * 1.3f);
            h.noiseEnv = 0.6f; h.noiseCoef = envFor (0.004f);
            h.pan = 0.0f;
            break;
        case 1:   // Amboss: freie Stange – unharmonische Teiltöne
        {
            static const float ratio[6] = { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f };
            static const float dmul[6]  = { 1.0f, 0.7f, 0.5f, 0.35f, 0.25f, 0.18f };
            static const float am[6]    = { 1.0f, 0.8f, 0.6f, 0.45f, 0.3f, 0.2f };
            h.baseHz = 420.0 * std::pow (2.0, tn);
            for (int k = 0; k < 6; ++k)
            {
                h.freq[k] = h.baseHz * ratio[k] * (1.0 + 0.004 * (rng.nextFloat() - 0.5f));
                h.amp[k]  = am[k] * (h.freq[k] < 0.45 * sr ? 1.0f : 0.0f);
                h.coef[k] = envFor ((0.15f + dc * 2.5f) * dmul[k]);
                h.ph[k] = rng.nextDouble();
            }
            h.noiseEnv = 0.5f; h.noiseCoef = envFor (0.006f);
            h.f1.set (4200.0, sr, 1.5f);
            h.pan = -0.3f;
            break;
        }
        case 2:   // Blasebalg: Rauschstoß mit Körper (snare-artig) und Atem
            h.baseHz = 165.0 * std::pow (2.0, tn);
            h.f1.set (700.0 * std::pow (2.0, tn * 1.5), sr, 1.2f);
            h.f2.set (2500.0, sr, 0.7f);
            h.envCoef = envFor (0.08f + dc * 0.9f);
            h.noiseEnv = 1.0f; h.noiseCoef = envFor (0.25f + dc * 1.2f);
            h.pan = 0.2f;
            break;
        case 3:   // Esse: knisterndes Rauschen, hell
            h.f1.set (6500.0 * std::pow (2.0, tn * 0.7), sr, 0.7f);
            h.f2.set (9500.0 * std::pow (2.0, tn * 0.5), sr, 4.0f);
            h.envCoef = envFor (0.015f + dc * 0.4f);
            h.pan = 0.45f;
            break;
        case 4:   // Funke: FM-Zischer mit zufälligem Verhältnis
            h.baseHz = 1500.0 * std::pow (2.0, tn * 1.5);
            h.fmRatio = 1.41f + rng.nextFloat() * 2.3f;
            h.fmIndex = 6.0f * vel; h.fmCoef = envFor (0.03f);
            h.envCoef = envFor (0.01f + dc * 0.25f);
            h.pan = (rng.nextFloat() * 2.0f - 1.0f) * 0.7f;
            break;
        default:  // Zange: metallischer Klick
            h.baseHz = 900.0 * std::pow (2.0, tn);
            h.fmRatio = 2.31f; h.fmIndex = 3.0f; h.fmCoef = envFor (0.005f);
            h.f1.set (2200.0 * std::pow (2.0, tn), sr, 6.0f);
            h.envCoef = envFor (0.01f + dc * 0.15f);
            h.noiseEnv = 0.8f; h.noiseCoef = envFor (0.003f);
            h.pan = -0.5f;
            break;
    }
    hitFlash[t] = 1.0f;
}

float HammerProcessor::renderHit (Hit& h) noexcept
{
    const float n = rng.nextFloat() * 2.0f - 1.0f;
    float y = 0.0f;
    switch (h.type)
    {
        case 0:
        {
            const double f = h.baseHz * (1.0 + 5.0 * std::exp (-h.age / (0.025 * sr)));
            h.ph[0] += f / sr; if (h.ph[0] >= 1.0) h.ph[0] -= 1.0;
            y = std::tanh (1.6f * tmk::dsp::sine (h.ph[0])) * h.env + n * h.noiseEnv * 0.5f;
            h.env *= h.envCoef; h.noiseEnv *= h.noiseCoef;
            break;
        }
        case 1:
        {
            for (int k = 0; k < 6; ++k)
            {
                if (h.amp[k] < 1.0e-5f) continue;
                y += tmk::dsp::sine (h.ph[k]) * h.amp[k];
                h.ph[k] += h.freq[k] / sr; if (h.ph[k] >= 1.0) h.ph[k] -= 1.0;
                h.amp[k] *= h.coef[k];
            }
            y = y * 0.3f + h.f1.bandpass (n) * h.noiseEnv;
            h.noiseEnv *= h.noiseCoef;
            h.env = h.amp[0] + h.noiseEnv;
            break;
        }
        case 2:
        {
            h.ph[0] += h.baseHz / sr; if (h.ph[0] >= 1.0) h.ph[0] -= 1.0;
            const float body = tmk::dsp::sine (h.ph[0]) * h.env;
            y = body * 0.6f + h.f1.bandpass (n) * h.env * 0.9f + h.f2.lowpass (n) * h.noiseEnv * 0.12f;
            h.env *= h.envCoef; h.noiseEnv *= h.noiseCoef;
            h.env = juce::jmax (h.env, 0.0f);
            if (h.noiseEnv > h.env) h.env = juce::jmax (h.env, h.noiseEnv * 0.2f);
            break;
        }
        case 3:
        {
            float l, b, hp;
            h.f1.process (n, l, b, hp);
            const float crackle = rng.nextFloat() < 0.08f ? 1.6f : 0.6f;          // Knistern
            y = (hp * 0.7f + h.f2.bandpass (hp) * 0.6f) * h.env * crackle;
            h.env *= h.envCoef;
            break;
        }
        case 4:
        {
            const double fm = h.baseHz * h.fmRatio;
            h.ph[1] += fm / sr; if (h.ph[1] >= 1.0) h.ph[1] -= 1.0;
            const double mod = h.fmIndex * tmk::dsp::sine (h.ph[1]);
            h.ph[0] += (h.baseHz / sr) * (1.0 + mod * 0.5); h.ph[0] -= std::floor (h.ph[0]);
            y = tmk::dsp::sine (h.ph[0]) * h.env * 0.6f;
            h.fmIndex *= h.fmCoef; h.env *= h.envCoef;
            break;
        }
        default:
        {
            h.ph[1] += h.baseHz * h.fmRatio / sr; if (h.ph[1] >= 1.0) h.ph[1] -= 1.0;
            h.ph[0] += (h.baseHz / sr) * (1.0 + h.fmIndex * 0.4 * tmk::dsp::sine (h.ph[1])); h.ph[0] -= std::floor (h.ph[0]);
            y = tmk::dsp::sine (h.ph[0]) * h.env * 0.5f + h.f1.bandpass (n) * h.noiseEnv * 1.4f;
            h.fmIndex *= h.fmCoef; h.env *= h.envCoef; h.noiseEnv *= h.noiseCoef;
            break;
        }
    }
    ++h.age;
    if (h.env < 1.0e-4f && h.noiseEnv < 1.0e-4f && h.age > 32) h.active = false;
    return y * (0.4f + 0.6f * h.vel);
}

//==============================================================================
void HammerProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // --- Takt ---
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying() && pos->getPpqPosition().hasValue()) { ppq = *pos->getPpqPosition(); playing = true; }
        }
    const double bps = bpm / 60.0 / sr;
    const bool running = playing || frei->load() > 0.5f;
    if (playing) ppq += chunkOffset * bps;
    else ppq = freePpq;

    const float ch = chaos->load(), sw = swing->load(), mut = mutation->load();
    const float glutAmt = glut->load(), stotterP = stotter->load();

    if (running)
    {
        // Mutation: pro Takt wandern Drehung und Schlagzahl ein wenig – und streben wieder zurück
        const int bar = (int) std::floor (ppq / 4.0);
        if (bar != lastBar)
        {
            lastBar = bar;
            for (int t = 0; t < numTracks; ++t)
            {
                if (rng.nextFloat() < mut * 0.6f) rotOff[t] += rng.nextBool() ? 1 : -1;
                if (rng.nextFloat() < mut * 0.4f) hitOff[t] = juce::jlimit (-3, 3, hitOff[t] + (rng.nextBool() ? 1 : -1));
                if (rng.nextFloat() < (1.0f - mut) * 0.3f) { rotOff[t] -= juce::jlimit (-1, 1, rotOff[t]); hitOff[t] -= juce::jlimit (-1, 1, hitOff[t]); }
                rotOffset[t] = rotOff[t]; hitOffset[t] = hitOff[t];
            }
        }

        // Schritte jeder Spur in diesem Block finden (jede Spur hat ihr eigenes Tempo)
        for (int t = 0; t < numTracks; ++t)
        {
            const double stepBeats = 0.25 / rateFactor ((int) rate[t]->load());
            const double stepSamples = stepBeats / bps;
            const double p0 = ppq / stepBeats, p1 = (ppq + n * bps) / stepBeats;
            const int st = juce::jlimit (2, 32, (int) std::round (steps[t]->load()));
            const int hc = juce::jlimit (0, st, (int) std::round (hitsP[t]->load()) + hitOff[t]);
            const int ro = (int) std::round (rotate[t]->load()) + rotOff[t];
            for (long long k = (long long) std::ceil (p0); (double) k < p1; ++k)
            {
                const int idx = (int) (((k % st) + st) % st);
                curStep[t] = idx;
                if (! euclid (idx, st, hc, ro)) continue;
                if (rng.nextFloat() > chance[t]->load()) continue;
                double off = (k * stepBeats - ppq) / bps;
                if (k % 2 != 0) off += sw * stepSamples * 0.5;                                  // Swing
                off += juce::jmax (0.0, (rng.nextDouble() - 0.3) * ch * stepSamples * 0.35);    // Chaos: Timing
                const float vel = (idx == 0 ? 1.0f : 0.7f + 0.3f * rng.nextFloat()) * (1.0f - 0.3f * ch * rng.nextFloat());
                pending.push_back ({ (int) off, vel, t });
                // Chaos: Wirbel (Ratschen) – schnelle Wiederholungen innerhalb des Schritts
                if (rng.nextFloat() < ch * 0.3f)
                {
                    const int rolls = 1 + rng.nextInt (3);
                    for (int r = 1; r <= rolls; ++r)
                        pending.push_back ({ (int) (off + r * stepSamples / (rolls + 1)), vel * (0.8f - 0.15f * r), t });
                }
            }
        }
    }

    // MIDI: Noten C1..F1 (36-41) spielen die Spuren direkt
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn() && msg.getNoteNumber() >= 36 && msg.getNoteNumber() < 36 + numTracks)
            pending.push_back ({ m.samplePosition, msg.getFloatVelocity(), msg.getNoteNumber() - 36 });
    }

    // --- Klang erzeugen ---
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    float lvl[numTracks];
    for (int t = 0; t < numTracks; ++t) lvl[t] = level[t]->load();

    // Glut-Einstellungen
    const int recSize = (int) rec[0].size();
    const int grainLen = juce::jmax (32, (int) (korn->load() * 0.001f * (float) sr));
    const double grainRate = std::pow (2.0, std::round (tonhoehe->load()) / 12.0);
    const float dens = dichte->load(), rev = rueckwaerts->load(), scat = streuung->load();
    const double baseStepSamples = 0.25 / bps;

    for (int i = 0; i < n; ++i)
    {
        // fällige Schläge auslösen
        for (size_t e = 0; e < pending.size();)
        {
            if (pending[e].samples <= i)
            {
                trigger (pending[e].track, i, pending[e].vel);
                pending[e] = pending.back(); pending.pop_back();
            }
            else ++e;
        }

        float dl = 0.0f, dr = 0.0f;
        for (int t = 0; t < numTracks; ++t)
            for (auto& h : hits[t])
                if (h.active)
                {
                    const float y = renderHit (h) * lvl[t];
                    dl += y * std::sqrt (0.5f * (1.0f - h.pan));
                    dr += y * std::sqrt (0.5f * (1.0f + h.pan));
                }

        // Aufnahme für die Glut
        rec[0][(size_t) recW] = dl; rec[1][(size_t) recW] = dr;

        float gl = 0.0f, gr = 0.0f, sl = 0.0f, srr = 0.0f;
        if (glutAmt > 0.001f)
        {
            // Granular-Wolke aus den letzten Sekunden
            untilGrain -= 1.0;
            if (untilGrain <= 0.0 && dens > 0.01f)
            {
                untilGrain += grainLen / (0.5 + 6.0 * dens) * (0.7 + 0.6 * rng.nextDouble());
                for (auto& g : grains)
                    if (! g.active)
                    {
                        const bool back = rng.nextFloat() < rev;
                        const double r = back ? -grainRate : grainRate;
                        const double need = grainLen * std::abs (r) + 64.0;
                        double start = recW - need - scat * rng.nextDouble() * (recSize - need * 2.0 - 64.0) * 0.6;
                        if (back) start += grainLen * std::abs (r);
                        g.active = true; g.pos = start; g.step = r; g.age = 0; g.len = grainLen;
                        const float p = (rng.nextFloat() * 2.0f - 1.0f) * 0.8f;
                        g.gl = std::sqrt (0.5f * (1.0f - p)); g.gr = std::sqrt (0.5f * (1.0f + p));
                        break;
                    }
            }
            for (auto& g : grains)
            {
                if (! g.active) continue;
                double rp = g.pos; while (rp < 0.0) rp += recSize; while (rp >= recSize) rp -= recSize;
                const int i0 = (int) rp, i1 = (i0 + 1) % recSize;
                const float fr = (float) (rp - i0);
                const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) g.age / (float) g.len);
                const float v = 0.5f * ((rec[0][(size_t) i0] + fr * (rec[0][(size_t) i1] - rec[0][(size_t) i0]))
                                      + (rec[1][(size_t) i0] + fr * (rec[1][(size_t) i1] - rec[1][(size_t) i0]))) * w;
                gl += v * g.gl; gr += v * g.gr;
                g.pos += g.step;
                if (++g.age >= g.len) g.active = false;
            }
            gl *= 0.7f; gr *= 0.7f;

            // Stotter: auf dem 16tel-Raster einen Schnipsel einfangen und wiederholen (mit Tonhöhen-Treppe)
            if (running)
            {
                const int baseStep = (int) std::floor ((ppq + i * bps) / 0.25);
                if (baseStep != lastBaseStep)
                {
                    lastBaseStep = baseStep;
                    if (! stut.active && rng.nextFloat() < stotterP * 0.35f)
                    {
                        const double frac[] = { 1.0, 0.5, 0.25, 0.75 };
                        stut.active = true;
                        stut.sliceLen = juce::jmax (64, (int) (baseStepSamples * frac[rng.nextInt (4)]));
                        stut.sliceStart = recW - stut.sliceLen;
                        stut.repeats = 2 + rng.nextInt (6);
                        stut.rpos = 0.0; stut.rate = 1.0; stut.gain = 1.0f;
                    }
                }
            }
            if (stut.active)
            {
                const double rp0 = stut.sliceStart + stut.rpos;
                double rp = rp0; while (rp < 0.0) rp += recSize; while (rp >= recSize) rp -= recSize;
                const int i0 = (int) rp, i1 = (i0 + 1) % recSize;
                const float fr = (float) (rp - i0);
                const float fade = (float) juce::jmin (1.0, stut.rpos / 96.0, (stut.sliceLen - stut.rpos) / 96.0);
                sl  = (rec[0][(size_t) i0] + fr * (rec[0][(size_t) i1] - rec[0][(size_t) i0])) * fade * stut.gain;
                srr = (rec[1][(size_t) i0] + fr * (rec[1][(size_t) i1] - rec[1][(size_t) i0])) * fade * stut.gain;
                stut.rpos += stut.rate;
                if (stut.rpos >= stut.sliceLen)
                {
                    stut.rpos = 0.0;
                    stut.gain *= 0.9f;
                    stut.rate *= std::pow (2.0, std::round (tonhoehe->load()) / 12.0 * 0.25);   // jede Wiederholung höher/tiefer
                    if (ch > 0.4f) stut.sliceLen = juce::jmax (64, (int) (stut.sliceLen * 0.8));  // bei viel Chaos: immer kürzer
                    if (--stut.repeats <= 0) stut.active = false;
                }
            }
        }
        if (++recW >= recSize) recW = 0;

        stutMix += ((stut.active ? 1.0f : 0.0f) - stutMix) * 0.01f;
        const float dryG = 1.0f - glutAmt * (0.3f + 0.7f * stutMix);
        L[i] = dl * dryG + (gl * (1.0f - stutMix) + sl * stutMix) * glutAmt;
        R[i] = dr * dryG + (gr * (1.0f - stutMix) + srr * stutMix) * glutAmt;
    }

    // ausstehende Schläge in den nächsten Block verschieben
    for (auto& e : pending) e.samples -= n;
    if (pending.size() > 200) pending.clear();
    if (! playing) freePpq = ppq + n * bps;
    stuttering = stut.active;
    for (auto& f : hitFlash) f = f.load() * 0.85f;

    // Hitze (Sättigung) und Lautstärke
    const float heat = hitze->load();
    const float drive = 1.0f + 6.0f * heat;
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSm.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            float& x = buffer.getWritePointer (c)[i];
            const float hot = x * (1.0f - heat) + std::tanh (x * drive) * heat * 0.75f;
            x = std::tanh (hot * g);
        }
    }
}

juce::AudioProcessorEditor* HammerProcessor::createEditor() { return new HammerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HammerProcessor(); }

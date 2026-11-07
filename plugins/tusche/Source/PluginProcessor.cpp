#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <deque>

using namespace tusche;

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using PB = juce::AudioParameterBool;
    using PI = juce::AudioParameterInt; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<PC> (ID { "modus", 1 },    "Modus", modes, 0));
    p.push_back (std::make_unique<PB> (ID { "sync", 1 },     "Sync", true));
    p.push_back (std::make_unique<PC> (ID { "schritt", 1 },  u8 ("Schrittl\xc3\xa4nge"), stepLens, 3));
    p.push_back (std::make_unique<PI> (ID { "auswahl", 1 },  "Strich", 1, numChops, 1));
    p.push_back (std::make_unique<PC> (ID { "quelle", 1 },   "Quelle", sources, 0));
    p.push_back (std::make_unique<P>  (ID { "wandern", 1 },  "Wandern", -1.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "attack", 1 },   "Attack (ms)", tmk::skewed (1.0f, 3000.0f, 120.0f), 15.0f));
    p.push_back (std::make_unique<P>  (ID { "decay", 1 },    "Decay (ms)", tmk::skewed (5.0f, 4000.0f, 400.0f), 400.0f));
    p.push_back (std::make_unique<P>  (ID { "sustain", 1 },  "Sustain", 0.0f, 1.0f, 0.8f));
    p.push_back (std::make_unique<P>  (ID { "release", 1 },  "Release (ms)", tmk::skewed (5.0f, 6000.0f, 600.0f), 600.0f));
    p.push_back (std::make_unique<P>  (ID { "tief", 1 },     "Tiefpass (Hz)", tmk::skewed (200.0f, 20000.0f, 3000.0f), 20000.0f));
    p.push_back (std::make_unique<P>  (ID { "hoch", 1 },     "Hochpass (Hz)", tmk::skewed (20.0f, 5000.0f, 300.0f), 20.0f));
    p.push_back (std::make_unique<P>  (ID { "reso", 1 },     "Resonanz", 0.0f, 1.0f, 0.15f));
    p.push_back (std::make_unique<P>  (ID { "raumgr", 1 },   u8 ("Raumgr\xc3\xb6\xc3\x9f" "e"), 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "ausgang", 1 },  "Ausgang (dB)", -24.0f, 12.0f, -3.0f));
    for (int c = 0; c < numChops; ++c)
    {
        const juce::String n = "Strich " + juce::String (c + 1);
        p.push_back (std::make_unique<P>  (ID { cid (c, "pos"), 1 },    n + " Position", 0.0f, 1.0f, (c + 0.3f) / numChops));
        p.push_back (std::make_unique<P>  (ID { cid (c, "korn"), 1 },   n + u8 (" Korngr\xc3\xb6\xc3\x9f" "e (ms)"), tmk::skewed (5.0f, 1000.0f, 100.0f), 90.0f));
        p.push_back (std::make_unique<P>  (ID { cid (c, "dichte"), 1 }, n + " Dichte", 0.0f, 1.0f, 0.62f));
        p.push_back (std::make_unique<P>  (ID { cid (c, "chaos"), 1 },  n + " Chaos", 0.0f, 1.0f, 0.15f));
        p.push_back (std::make_unique<P>  (ID { cid (c, "raum"), 1 },   n + " Raum", 0.0f, 1.0f, 0.25f));
        p.push_back (std::make_unique<PI> (ID { cid (c, "ton"), 1 },    n + u8 (" Tonh\xc3\xb6he (HT)"), -24, 24, 0));
        p.push_back (std::make_unique<PB> (ID { cid (c, "an"), 1 },     n + " aktiv", true));
    }
    return { p.begin(), p.end() };
}

//==============================================================================
// Bibliothek: Presets werden aus kleinen Rezepten gebaut
namespace
{
    std::deque<std::string>& idStore() { static std::deque<std::string> s; return s; }
    const char* keepId (const juce::String& s) { idStore().push_back (s.toStdString()); return idStore().back().c_str(); }

    struct Recipe
    {
        const char* name;
        std::vector<std::pair<const char*, float>> global;
        std::function<void (int chop, juce::Random&, float* v /* pos korn dichte chaos raum ton an */)> chop;
    };

    std::vector<tmk::Preset> buildPresets (const std::vector<Recipe>& recipes)
    {
        std::vector<tmk::Preset> out;
        for (auto& r : recipes)
        {
            tmk::Preset p { r.name, r.global };
            if (r.chop)
            {
                juce::Random rnd (4711);
                for (int c = 0; c < numChops; ++c)
                {
                    float v[7] = { (c + 0.3f) / numChops, 90.0f, 0.62f, 0.15f, 0.25f, 0.0f, 1.0f };
                    r.chop (c, rnd, v);
                    for (int k = 0; k < numChopParams; ++k) p.values.push_back ({ keepId (cid (c, chopParams[k])), v[k] });
                    p.values.push_back ({ keepId (cid (c, "an")), v[6] });
                }
            }
            out.push_back (p);
        }
        return out;
    }

    std::vector<tmk::Preset> factoryPresets()
    {
        const int penta[] = { 0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24, -3, -5, -8, -10, -12 };
        return buildPresets ({
            { "Grundierung", {}, nullptr },
            { "Lavierung \xc2\xb7 Tuschnebel",
              { { "quelle", 0 }, { "modus", 0 }, { "attack", 400.0f }, { "release", 2500.0f }, { "raumgr", 0.7f }, { "wandern", 0.1f } },
              [] (int, juce::Random& r, float* v) { v[1] = 300.0f; v[2] = 0.78f; v[3] = 0.35f + 0.2f * r.nextFloat(); v[4] = 0.6f; } },
            { "Spritzer \xc2\xb7 Glasstaub",
              { { "quelle", 1 }, { "modus", 0 }, { "sync", 0 }, { "attack", 200.0f }, { "release", 1800.0f } },
              [] (int, juce::Random& r, float* v) { v[1] = 35.0f; v[2] = 0.85f; v[3] = 0.6f; v[4] = 0.45f + 0.2f * r.nextFloat(); } },
            { "Druck \xc2\xb7 Pinselschl\xc3\xa4ge",
              { { "quelle", 3 }, { "modus", 1 }, { "schritt", 3 }, { "attack", 2.0f }, { "decay", 160.0f }, { "sustain", 0.25f }, { "release", 120.0f } },
              [] (int c, juce::Random& r, float* v) { v[0] = r.nextFloat(); v[1] = 60.0f; v[2] = 0.66f; v[3] = 0.08f; v[4] = 0.15f;
                                                      v[5] = (float) ((c % 4 == 2) ? 12 : (c % 8 == 5 ? 7 : 0)); v[6] = (c % 4 == 3 && c != 15) ? 0.0f : 1.0f; } },
            { "Druck \xc2\xb7 Stempel",
              { { "quelle", 0 }, { "modus", 1 }, { "schritt", 1 }, { "attack", 3.0f }, { "decay", 300.0f }, { "sustain", 0.5f }, { "release", 200.0f } },
              [] (int c, juce::Random&, float* v) { v[0] = (float) ((c * 5) % 16) / 16.0f; v[1] = 120.0f; v[2] = 0.7f; v[3] = 0.1f; v[4] = 0.2f; } },
            { "Lavierung \xc2\xb7 Leeres Blatt",
              { { "quelle", 2 }, { "modus", 0 }, { "sync", 0 }, { "attack", 1500.0f }, { "release", 4500.0f }, { "raumgr", 0.9f },
                { "wandern", 0.15f }, { "tief", 6000.0f } },
              [] (int, juce::Random&, float* v) { v[1] = 600.0f; v[2] = 0.5f; v[3] = 0.25f; v[4] = 0.85f; } },
            { "Spritzer \xc2\xb7 Klecks",
              { { "quelle", 3 }, { "modus", 1 }, { "schritt", 4 }, { "attack", 1.0f }, { "decay", 90.0f }, { "sustain", 0.0f }, { "release", 60.0f } },
              [] (int, juce::Random& r, float* v) { v[0] = r.nextFloat(); v[1] = 25.0f + 60.0f * r.nextFloat(); v[2] = 0.75f + 0.2f * r.nextFloat();
                                                    v[3] = 0.8f; v[4] = 0.3f; v[5] = (float) (r.nextInt (25) - 12); v[6] = r.nextFloat() < 0.7f ? 1.0f : 0.0f; } },
            { "Grund \xc2\xb7 Nasse Lavierung",
              { { "quelle", 2 }, { "modus", 0 }, { "attack", 800.0f }, { "sustain", 1.0f }, { "release", 3000.0f }, { "raumgr", 0.75f } },
              [] (int, juce::Random&, float* v) { v[1] = 450.0f; v[2] = 0.82f; v[3] = 0.15f; v[4] = 0.7f; } },
            { "Strich \xc2\xb7 Feder",
              { { "quelle", 1 }, { "modus", 0 }, { "attack", 10.0f }, { "release", 400.0f }, { "sustain", 0.9f } },
              [] (int, juce::Random&, float* v) { v[1] = 70.0f; v[2] = 0.9f; v[3] = 0.05f; v[4] = 0.3f; } },
            { "Strich \xc2\xb7 Tastenstriche",
              { { "quelle", 0 }, { "modus", 2 }, { "attack", 5.0f }, { "decay", 800.0f }, { "sustain", 0.6f }, { "release", 900.0f } },
              [] (int c, juce::Random&, float* v) { v[0] = (c + 0.2f) / numChops; v[1] = 150.0f; v[2] = 0.7f; v[3] = 0.1f; v[4] = 0.35f; } },
            { "Strich \xc2\xb7 Glasharfe",
              { { "quelle", 1 }, { "modus", 2 }, { "attack", 4.0f }, { "decay", 1200.0f }, { "sustain", 0.4f }, { "release", 1500.0f } },
              [penta] (int c, juce::Random&, float* v) { v[0] = 0.05f; v[1] = 200.0f; v[2] = 0.72f; v[3] = 0.05f; v[4] = 0.5f; v[5] = (float) penta[c]; } },
        });
    }
}

//==============================================================================
// Eingebaute Klangquellen (werden beim Start berechnet, 8 s Mono, 48 kHz)
std::unique_ptr<TuscheProcessor::Source> TuscheProcessor::makeBuiltIn (int which)
{
    constexpr double fs = 48000.0;
    const int len = (int) (fs * 8.0);
    auto s = std::make_unique<Source>();
    s->rate = fs; s->name = sources[which];
    s->data.setSize (1, len); s->data.clear();
    float* d = s->data.getWritePointer (0);
    juce::Random r (100 + which);
    auto mtof = [] (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); };
    const double twoPi = juce::MathConstants<double>::twoPi;

    if (which == 0)          // Klavier: gezupfte Saiten mit leichter Inharmonizität, Arpeggio durch vier Akkorde
    {
        const int chords[4][5] = { { 48, 55, 64, 71, 74 }, { 41, 53, 60, 64, 69 }, { 45, 52, 60, 67, 71 }, { 43, 50, 59, 62, 67 } };
        for (int k = 0; k < 20; ++k)
        {
            const double f0 = mtof (chords[(k / 5) % 4][k % 5]);
            const int start = (int) (k * 0.4 * fs);
            const double B = 0.0004;
            for (int h = 1; h <= 14; ++h)
            {
                const double f = f0 * h * std::sqrt (1.0 + B * h * h);
                if (f > 16000.0) break;
                const double a = 0.25 / std::pow (h, 1.15) * (h == 1 ? 1.0 : 0.9), tau = 2.6 / (1.0 + 0.35 * h);
                const double ph = r.nextDouble();
                for (int i = 0; start + i < len && i < (int) (fs * 3.2); ++i)
                {
                    const double t = i / fs;
                    d[start + i] += (float) (a * std::exp (-t / tau) * std::min (1.0, t / 0.002) * std::sin (twoPi * (f * t + ph)));
                }
            }
            for (int i = 0; i < 300 && start + i < len; ++i) d[start + i] += 0.05f * (r.nextFloat() * 2 - 1) * (1.0f - i / 300.0f);
        }
    }
    else if (which == 1)     // Glas: unharmonische Glockenteiltöne, lang ausklingend
    {
        const double ratio[] = { 1.0, 2.32, 4.25, 6.63, 9.38 };
        const int notes[] = { 72, 79, 76, 84, 74, 81, 77, 88 };
        for (int k = 0; k < 8; ++k)
        {
            const double f0 = mtof (notes[k]);
            const int start = (int) (k * 1.0 * fs);
            for (int m = 0; m < 5; ++m)
            {
                const double f = f0 * ratio[m];
                if (f > 18000.0) break;
                const double a = 0.22 / (1.0 + m * 0.8), tau = 3.5 / (1.0 + m * 0.9);
                const double beat = 0.7 + r.nextDouble();
                for (int i = 0; start + i < len; ++i)
                {
                    const double t = i / fs;
                    d[start + i] += (float) (a * std::exp (-t / tau) * std::min (1.0, t / 0.001) * std::sin (twoPi * f * t)
                                             * (1.0 - 0.15 * std::sin (twoPi * beat * t)));
                }
            }
        }
    }
    else if (which == 2)     // Chor: drei Stimmen, Formantfilter wandern über a-o-u-e
    {
        const double vowels[4][3] = { { 800, 1150, 2900 }, { 450, 800, 2830 }, { 325, 700, 2700 }, { 400, 1600, 2700 } };
        const int notes[3] = { 57, 64, 69 };
        for (int v = 0; v < 3; ++v)
        {
            tmk::dsp::Svf f[3];
            double ph = r.nextDouble();
            for (int i = 0; i < len; ++i)
            {
                const double t = i / fs;
                const double vib = 1.0 + 0.006 * std::sin (twoPi * (5.0 + v * 0.4) * t + v);
                const double f0 = mtof (notes[v] + (t > 4.0 ? (v == 1 ? 1 : 0) : 0)) * vib;
                ph += f0 / fs; ph -= std::floor (ph);
                const float saw = (float) (2.0 * ph - 1.0);
                const double pos = std::fmod (t / 2.0, 4.0);
                const int a = (int) pos, b = (a + 1) % 4;
                const double fr = pos - a;
                float y = 0.0f;
                if (i % 32 == 0)
                    for (int k = 0; k < 3; ++k)
                        f[k].set (vowels[a][k] + (vowels[b][k] - vowels[a][k]) * fr, fs, 6.0f + k * 2.0f);
                for (int k = 0; k < 3; ++k) y += f[k].bandpass (saw) * (k == 0 ? 1.0f : 0.5f);
                const float swell = (float) (0.5 + 0.5 * std::sin (twoPi * 0.12 * t + v * 2.0));
                d[i] += y * 0.25f * (0.5f + 0.5f * swell) * (float) std::min (1.0, t / 0.3);
            }
        }
    }
    else                     // Holz: Marimba-Schläge im 16tel-Raster, dazu tiefe Trommel
    {
        const int scale[] = { 57, 60, 62, 64, 67, 69, 72, 74 };
        const double step = 0.125;   // 16tel bei 120 BPM
        for (int k = 0; k < 64; ++k)
        {
            if (r.nextFloat() < 0.3f && k % 4 != 0) continue;
            const int start = (int) (k * step * fs);
            const bool drum = k % 8 == 0 || k % 16 == 11;
            const double f0 = drum ? 70.0 + r.nextInt (20) : mtof (scale[r.nextInt (8)]);
            const double ratio[] = { 1.0, 3.93, 9.16 };
            for (int m = 0; m < 3; ++m)
            {
                const double f = f0 * ratio[m], tau = (drum ? 0.25 : 0.4) / (1.0 + m * 3.0);
                const double a = (drum ? 0.6 : 0.3) / (1.0 + m * 1.5);
                for (int i = 0; start + i < len && i < (int) (fs * 1.5); ++i)
                {
                    const double t = i / fs;
                    const double fm = drum ? f * (1.0 + 0.8 * std::exp (-t / 0.02)) : f;
                    d[start + i] += (float) (a * std::exp (-t / tau) * std::sin (twoPi * fm * t));
                }
            }
            for (int i = 0; i < 200 && start + i < len; ++i) d[start + i] += 0.12f * (r.nextFloat() * 2 - 1) * (1.0f - i / 200.0f);
        }
    }
    const float pk = s->data.getMagnitude (0, len);
    if (pk > 0.0f) s->data.applyGain (0.9f / pk);
    return s;
}

//==============================================================================
TuscheProcessor::TuscheProcessor()
    : tmk::Processor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "TUSCHE", factoryPresets())
{
    auto get = [this] (const juce::String& i) { return apvts.getRawParameterValue (i); };
    modus = get ("modus"); sync = get ("sync"); schritt = get ("schritt"); auswahl = get ("auswahl"); quelle = get ("quelle");
    wandern = get ("wandern"); attack = get ("attack"); decay = get ("decay"); sustain = get ("sustain"); release = get ("release");
    tief = get ("tief"); hoch = get ("hoch"); reso = get ("reso"); raumGroesse = get ("raumgr"); ausgang = get ("ausgang");
    for (int c = 0; c < numChops; ++c)
    {
        for (int k = 0; k < numChopParams; ++k) chopP[c][k] = get (cid (c, chopParams[k]));
        chopOn[c] = get (cid (c, "an"));
    }
    for (int i = 0; i < 4; ++i) builtIn[i] = makeBuiltIn (i);
    formats.registerBasicFormats();
    heldNotes.reserve (128);
}

bool TuscheProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TuscheProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& v : voices) { v = Voice(); v.env.setSampleRate (sr); }
    for (auto& g : grains) g.active = false;
    keysHeld = 0; heldNotes.clear(); freeBeat = 0.0; lastStepIndex = -1;
    for (int c = 0; c < 2; ++c) { lp[c].reset(); hp[c].reset(); }
    reverb.prepare (sr);
    sendBuf.setSize (2, juce::jmax (samplesPerBlock, maxChunk));
    outSm.reset (sr, 0.05);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (ausgang->load()));
    keyboardState.reset();
}

//==============================================================================
// Samples
bool TuscheProcessor::isAudioFile (const juce::File& f)
{
    return f.existsAsFile() && f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

const TuscheProcessor::Source* TuscheProcessor::currentSource() const
{
    const int q = (int) quelle->load();
    if (q == 4) if (auto* u = userSrc.load()) return u;
    return builtIn[juce::jlimit (0, 3, q == 4 ? 0 : q)].get();
}

void TuscheProcessor::setUserSource (std::unique_ptr<Source> s)
{
    const auto now = juce::Time::getMillisecondCounter();
    retired.erase (std::remove_if (retired.begin(), retired.end(), [now] (const Retired& r) { return now - r.at > 20000; }), retired.end());
    userSrc = s.get();
    if (userOwner != nullptr) retired.push_back ({ std::move (userOwner), now });
    userOwner = std::move (s);
    apvts.state.setProperty ("sample", userOwner != nullptr ? userOwner->file.getFullPathName() : juce::String(), nullptr);
    ++sourceChanges;
}

juce::String TuscheProcessor::loadSample (const juce::File& file)
{
    if (! isAudioFile (file)) return "Keine Audiodatei: " + file.getFileName();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return "Kann nicht gelesen werden: " + file.getFileName();
    const auto len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));   // höchstens 60 s
    if (len < 64) return "Zu kurz: " + file.getFileName();
    juce::AudioBuffer<float> tmp ((int) juce::jmin (2u, reader->numChannels), len);
    reader->read (&tmp, 0, len, 0, true, tmp.getNumChannels() > 1);
    auto s = std::make_unique<Source>();
    s->data.setSize (1, len);
    s->data.copyFrom (0, 0, tmp, 0, 0, len);
    if (tmp.getNumChannels() > 1) { s->data.addFrom (0, 0, tmp, 1, 0, len); s->data.applyGain (0.5f); }
    const float pk = s->data.getMagnitude (0, len);
    if (pk > 1.0e-4f) s->data.applyGain (0.9f / pk);
    s->rate = reader->sampleRate; s->name = file.getFileNameWithoutExtension(); s->file = file;
    setUserSource (std::move (s));
    if (auto* q = apvts.getParameter ("quelle")) q->setValueNotifyingHost (q->convertTo0to1 (4.0f));
    return {};
}

void TuscheProcessor::clearSample()
{
    setUserSource (nullptr);
    if (auto* q = apvts.getParameter ("quelle")) q->setValueNotifyingHost (0.0f);
}

void TuscheProcessor::stateRestored()
{
    const juce::File f (apvts.state.getProperty ("sample").toString());
    if (f.getFullPathName().isNotEmpty() && f.existsAsFile() && (userOwner == nullptr || userOwner->file != f))
    {
        const float q = apvts.getParameter ("quelle")->getValue();
        loadSample (f);
        apvts.getParameter ("quelle")->setValueNotifyingHost (q);
    }
    else if (f.getFullPathName().isEmpty() && userOwner != nullptr) setUserSource (nullptr);
}

//==============================================================================
int TuscheProcessor::freeVoice()
{
    const int first = (int) modus->load() == 1 ? 1 : 0;     // Stimme 0 gehört im Schritt-Modus dem Sequenzer
    for (int i = first; i < maxVoices; ++i) if (! voices[i].active) return i;
    // die leiseste (bzw. älteste losgelassene) Stimme übernehmen
    int best = first; float lo = 2.0f;
    for (int i = first; i < maxVoices; ++i)
        if (! voices[i].keyDown && voices[i].envNow < lo) { lo = voices[i].envNow; best = i; }
    return best;
}

void TuscheProcessor::noteOn (int note, float vel)
{
    const int mode = (int) modus->load();
    lastNote = note;
    if (std::find (heldNotes.begin(), heldNotes.end(), note) == heldNotes.end()) heldNotes.push_back (note);
    if (mode == 1)                       // Schritt: Taste transponiert, Sequenzer startet ohne DAW ab Schritt 1
    {
        if (keysHeld == 0 && ! voices[0].active) { freeBeat = 0.0; lastStepIndex = -1; }
        ++keysHeld;
        voices[0].vel = vel;
        return;
    }
    if (mode == 0 && keysHeld == 0)      // Halten: neuer Akkord ersetzt den gehaltenen
        for (auto& v : voices) if (v.active) { v.env.noteOff(); v.keyDown = false; }
    ++keysHeld;
    auto& v = voices[freeVoice()];
    v.active = true; v.keyDown = true; v.note = note; v.vel = vel;
    if (mode == 0) { v.chop = juce::jlimit (0, numChops - 1, (int) auswahl->load() - 1); v.pitch = (float) (note - 60); }
    else            { v.chop = ((note - 60) % numChops + numChops) % numChops; v.pitch = 0.0f; }
    v.scan = 0.0; v.untilGrain = 0.0; v.gateLeft = -1;
    v.env.reset(); v.env.noteOn();
}

void TuscheProcessor::noteOff (int note)
{
    const int mode = (int) modus->load();
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    keysHeld = juce::jmax (0, keysHeld - 1);
    if (! heldNotes.empty()) lastNote = heldNotes.back();
    if (mode == 1) return;
    for (int i = 0; i < maxVoices; ++i)
        if (voices[i].active && voices[i].keyDown && voices[i].note == note)
        {
            voices[i].keyDown = false;
            if (mode == 2) voices[i].env.noteOff();      // Halten: klingt weiter (Latch)
        }
}

void TuscheProcessor::spawnGrain (int vi, const Source& src, double interval)
{
    int slot = -1;
    for (int i = 0; i < maxGrains; ++i) if (! grains[i].active) { slot = i; break; }
    if (slot < 0) return;
    auto& v = voices[vi];
    const int c = v.chop;
    const float chaos = chopValue (c, 3);
    const int L = src.data.getNumSamples();
    auto& g = grains[slot];

    double pos = chopValue (c, 0) + v.scan + chaos * (rng.nextDouble() * 2.0 - 1.0) * 0.08;
    pos -= std::floor (pos);
    int len = (int) (chopValue (c, 1) * 0.001 * sr * (1.0 + chaos * (rng.nextDouble() - 0.5) * 0.8));
    len = juce::jlimit (32, (int) (sr * 2.0), len);
    float semis = chopValue (c, 5) + v.pitch + chaos * 0.4f * (rng.nextFloat() - 0.5f);
    if (chaos > 0.55f && rng.nextFloat() < (chaos - 0.55f) * 0.9f) semis += rng.nextBool() ? 12.0f : -12.0f;   // Oktavsprünge
    double step = std::pow (2.0, semis / 12.0) * src.rate / sr;
    double start = pos * L;
    if (rng.nextFloat() < chaos * 0.3f) { start += len * step; step = -step; }                                 // rückwärts
    const float pan = (rng.nextFloat() * 2.0f - 1.0f) * (0.2f + 0.7f * chaos);

    // Lautstärkeausgleich: viele überlappende Körner -> leiser (gleich laute Presets)
    const double overlap = (double) len / juce::jmax (1.0, interval);
    g.active = true; g.voice = vi; g.age = 0; g.len = len;
    g.pos = start; g.step = step;
    g.gl = std::sqrt (0.5f * (1.0f - pan)); g.gr = std::sqrt (0.5f * (1.0f + pan));
    g.amp = v.vel / (float) std::sqrt (juce::jmax (1.0, overlap));
    g.send = chopValue (c, 4);
    g.viewPos = (float) pos;
    chopGlow[c] = 1.0f;
}

//==============================================================================
void TuscheProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    const Source* src = currentSource();
    const int L = src->data.getNumSamples();
    const float* sd = src->data.getReadPointer (0);

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying() && pos->getPpqPosition().hasValue()) { ppq = *pos->getPpqPosition(); playing = true; }
        }
    const double bps = bpm / 60.0 / sr;
    if (playing) ppq += chunkOffset * bps;

    const int mode = (int) modus->load();
    const bool synced = sync->load() > 0.5f;
    adsr.attack = attack->load() * 0.001f; adsr.decay = decay->load() * 0.001f;
    adsr.sustain = sustain->load(); adsr.release = release->load() * 0.001f;
    for (auto& v : voices) v.env.setParameters (adsr);
    const double scanPerSample = wandern->load() * 0.12 / sr;
    const double stepLen = stepBeats ((int) schritt->load());

    // Filter
    const float q = 0.6f + 5.5f * reso->load() * reso->load();
    for (int c = 0; c < 2; ++c) { lp[c].set (tief->load(), sr, q); hp[c].set (hoch->load(), sr, 0.7f); }
    tmk::FdnReverb::Settings rs;
    rs.sizeScale = 0.6f + 1.8f * raumGroesse->load(); rs.decay = 0.8f + 7.0f * raumGroesse->load(); rs.highCut = 6000.0f; rs.width = 1.0f;
    reverb.update (rs);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (ausgang->load()));

    auto* L0 = buffer.getWritePointer (0);
    auto* R0 = buffer.getWritePointer (1);
    auto* SL = sendBuf.getWritePointer (0);
    auto* SR = sendBuf.getWritePointer (1);
    auto mit = midi.cbegin();
    int voiceGrains[maxVoices];

    for (int i = 0; i < n; ++i)
    {
        for (; mit != midi.cend() && (*mit).samplePosition <= i; ++mit)
        {
            const auto m = (*mit).getMessage();
            if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) noteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                for (auto& v : voices) { v.env.noteOff(); v.keyDown = false; }
                heldNotes.clear(); keysHeld = 0;
            }
        }
        const double beat = playing ? ppq + i * bps : freeBeat + i * bps;

        // Schritt-Sequenzer (Stimme 0)
        if (mode == 1)
        {
            auto& v = voices[0];
            const bool run = playing || keysHeld > 0;
            if (run)
            {
                const long long idx = (long long) std::floor (beat / stepLen);
                if (idx != lastStepIndex)
                {
                    lastStepIndex = idx;
                    const int c = (int) (((idx % numChops) + numChops) % numChops);
                    currentStep = c;
                    v.chop = c;
                    v.pitch = keysHeld > 0 ? (float) (lastNote - 60) : 0.0f;
                    if (chopOn[c]->load() > 0.5f)
                    {
                        v.active = true;
                        v.env.noteOn();
                        v.gateLeft = (int) (stepLen / bps * 0.85);
                        v.untilGrain = 0.0;
                    }
                    else v.env.noteOff();
                }
            }
            else if (lastStepIndex != -1)
            {
                lastStepIndex = -1; currentStep = -1;
                v.env.noteOff();
            }
            if (v.gateLeft > 0 && --v.gateLeft == 0) v.env.noteOff();
        }
        else currentStep = -1;

        // Stimmen: Hüllkurve, Wandern, Körner planen
        for (int vi = 0; vi < maxVoices; ++vi)
        {
            auto& v = voices[vi];
            voiceGrains[vi] = 0;
            if (! v.active) { v.envNow = 0.0f; continue; }
            v.envNow = v.env.getNextSample();
            v.scan += scanPerSample;
            if (! v.env.isActive()) continue;
            const float dens = chopValue (v.chop, 2);
            double interval;
            if (synced) interval = divBeats (juce::roundToInt (dens * 8.0f)) / bps;
            else        interval = sr / (1.0 + dens * dens * 79.0);
            v.untilGrain -= 1.0;
            if (v.untilGrain <= 0.0)
            {
                spawnGrain (vi, *src, interval);
                const float chaos = chopValue (v.chop, 3);
                v.untilGrain += interval * (synced ? 1.0 : 1.0 + chaos * (rng.nextDouble() - 0.5));
                if (v.untilGrain < 1.0) v.untilGrain = 1.0;
            }
        }

        // Körner spielen
        float dl = 0.0f, dr = 0.0f, sl = 0.0f, srr = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            ++voiceGrains[g.voice];
            const float t = (float) g.age / (float) g.len;
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);
            double p = g.pos;
            p -= std::floor (p / L) * L;
            const int i0 = (int) p, i1 = i0 + 1 >= L ? 0 : i0 + 1;
            const float x = (sd[i0] + (float) (p - i0) * (sd[i1] - sd[i0])) * w * g.amp * voices[g.voice].envNow;
            dl += x * g.gl; dr += x * g.gr;
            sl += x * g.gl * g.send; srr += x * g.gr * g.send;
            g.pos += g.step;
            if (++g.age >= g.len) g.active = false;
        }
        for (int vi = 0; vi < maxVoices; ++vi)
        {
            auto& v = voices[vi];
            if (v.active && ! v.env.isActive() && voiceGrains[vi] == 0) v.active = false;
        }

        // Filter (Hochpass, dann Tiefpass mit Resonanz)
        float l, b, h;
        hp[0].process (dl, l, b, h); dl = h;  hp[1].process (dr, l, b, h); dr = h;
        lp[0].process (dl, l, b, h); dl = l;  lp[1].process (dr, l, b, h); dr = l;
        L0[i] = dl * 1.1f; R0[i] = dr * 1.1f;
        SL[i] = sl * 1.1f; SR[i] = srr * 1.1f;
    }
    if (! playing) freeBeat += n * bps;

    // Raum (Hall) dazu
    reverb.process (SL, SR, SL, SR, n);
    for (int i = 0; i < n; ++i)
    {
        const float og = outSm.getNextValue();
        L0[i] = std::tanh ((L0[i] + SL[i] * 0.8f) * og);
        R0[i] = std::tanh ((R0[i] + SR[i] * 0.8f) * og);
    }

    // Anzeige
    for (int gi = 0; gi < maxGrains; ++gi)
    {
        const auto& g = grains[gi];
        if (g.active)
        {
            const float t = (float) g.age / (float) g.len;
            double p = g.pos; p -= std::floor (p / L) * L;
            grainView[gi].pos = (float) (p / L);
            grainView[gi].glow = std::sin (juce::MathConstants<float>::pi * t);
            grainView[gi].pan = g.gr - g.gl;
        }
        else grainView[gi].glow = grainView[gi].glow.load() * 0.7f;
    }
    for (auto& cg : chopGlow) cg = cg.load() * 0.9f;
}

juce::AudioProcessorEditor* TuscheProcessor::createEditor() { return new TuscheEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TuscheProcessor(); }

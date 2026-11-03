#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <deque>

using namespace pascher;

// Muster als Zeichenketten: 'x' = an
static std::vector<std::pair<const char*, float>> pattern (const char* gate, const char* clap)
{
    static std::deque<std::string> keep;    // IDs müssen leben, solange das Preset existiert (deque: Zeiger bleiben gültig)
    std::vector<std::pair<const char*, float>> v;
    for (int s = 0; s < numSteps; ++s)
    {
        keep.push_back (gateId (s).toStdString());
        v.push_back ({ keep.back().c_str(), gate[s] == 'x' ? 1.0f : 0.0f });
    }
    for (int s = 0; s < numSteps; ++s)
    {
        keep.push_back (clapId (s).toStdString());
        v.push_back ({ keep.back().c_str(), clap[s] == 'x' ? 1.0f : 0.0f });
    }
    return v;
}

static std::vector<std::pair<const char*, float>> with (std::vector<std::pair<const char*, float>> a,
                                                        std::initializer_list<std::pair<const char*, float>> b)
{
    for (auto& e : b) a.push_back (e);
    return a;
}

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using PB = juce::AudioParameterBool;
    using PI = juce::AudioParameterInt; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<PC> (ID { "rate", 1 },      "Raster", rates, 0));
    p.push_back (std::make_unique<PI> (ID { "laenge", 1 },    u8 ("L\xc3\xa4nge"), 2, numSteps, 12));
    p.push_back (std::make_unique<P>  (ID { "swing", 1 },     "Swing", 0.0f, 0.6f, 0.08f));
    p.push_back (std::make_unique<P>  (ID { "schmuggel", 1 }, "Schmuggel", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "tiefe", 1 },     "Gate-Tiefe", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "glaetten", 1 },  u8 ("Gl\xc3\xa4tten (ms)"), tmk::skewed (0.5f, 60.0f, 8.0f), 6.0f));
    p.push_back (std::make_unique<P>  (ID { "mix", 1 },       "Gate-Mix", 0.0f, 1.0f, 1.0f));
    p.push_back (std::make_unique<P>  (ID { "klatsch", 1 },   "Klatschen", 0.0f, 1.0f, 0.7f));
    p.push_back (std::make_unique<PI> (ID { "klatscher", 1 }, "Klatscher", 1, 6, 3));
    p.push_back (std::make_unique<P>  (ID { "mensch", 1 },    "Mensch", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<P>  (ID { "ton", 1 },       u8 ("H\xc3\xa4nde"), 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "raum", 1 },      "Wirtshaus", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "output", 1 },    "Ausgang (dB)", -24.0f, 12.0f, 0.0f));
    const char* defClap = "x.xxx.x.xxx.....";
    for (int s = 0; s < numSteps; ++s)
        p.push_back (std::make_unique<PB> (ID { gateId (s), 1 }, "Gate " + juce::String (s + 1), true));
    for (int s = 0; s < numSteps; ++s)
        p.push_back (std::make_unique<PB> (ID { clapId (s), 1 }, "Klatschen " + juce::String (s + 1), defClap[s] == 'x'));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    const char* allOn = "xxxxxxxxxxxxxxxx";
    return {
        { "Goiserer Pasch",       pattern (allOn, "x.xxx.x.xxx.....") },
        { "Ausseer Dreier",       pattern (allOn, "xxx.x.xxx.x.....") },
        { "Gro\xc3\x9f" "er Paschertisch", with (pattern (allOn, "x.xxx.x.xxx....."), { { "klatscher", 6 }, { "mensch", 0.7f }, { "raum", 0.55f } }) },
        { "L\xc3\xa4ndler-Gate",  with (pattern ("x.x.x.x.x.x.....", "................"), { { "tiefe", 0.85f }, { "klatsch", 0.0f }, { "glaetten", 10.0f } }) },
        { "Trance-Gate 16",       with (pattern ("x.xxx.x.x.xx.x.x", "................"), { { "rate", 2 }, { "laenge", 16 }, { "tiefe", 1.0f },
                                                                                           { "klatsch", 0.0f }, { "swing", 0.0f }, { "glaetten", 3.0f } }) },
        { "Schmuggler-Pfad",      with (pattern ("xx.xx.xx.xx.....", "x..x..x..x......"), { { "schmuggel", 0.45f }, { "tiefe", 0.7f }, { "mensch", 0.5f } }) },
        { "Polka-Paschen",        with (pattern (allOn, "x.x.x.x.x.x.x.x."), { { "laenge", 16 }, { "rate", 0 }, { "swing", 0.15f } }) },
        { "Leises Mitklatschen",  with (pattern (allOn, "x.xxx.x.xxx....."), { { "klatsch", 0.35f }, { "klatscher", 2 }, { "raum", 0.6f } }) },
    };
}

PascherProcessor::PascherProcessor()
    : tmk::Processor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "GOISERER PASCHER", factoryPresets())
{
    auto get = [this] (const juce::String& i) { return apvts.getRawParameterValue (i); };
    rate = get ("rate"); laenge = get ("laenge"); swing = get ("swing"); schmuggel = get ("schmuggel"); tiefe = get ("tiefe");
    glaetten = get ("glaetten"); mix = get ("mix"); klatsch = get ("klatsch"); klatscher = get ("klatscher"); mensch = get ("mensch");
    ton = get ("ton"); raum = get ("raum"); output = get ("output");
    for (int s = 0; s < numSteps; ++s) { gates[s] = get (gateId (s)); clapSteps[s] = get (clapId (s)); }
}

bool PascherProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::disabled());
}

void PascherProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    freePpq = 0.0; lastStep = -1; gateGain = 1.0f;
    for (auto& c : claps) c.active = false;
    reverb.prepare (sr);
    clapBuf.setSize (2, samplesPerBlock);
    wet.setSize (2, samplesPerBlock);
    mixSm.reset (sr, 0.05);
    mixSm.setCurrentAndTargetValue (mix->load());
}

// Eine Gruppe von Klatschern: jeder mit eigener Verzögerung, Handgröße und Position im Raum
void PascherProcessor::triggerClaps (int offset, float accent)
{
    const int people = juce::jlimit (1, 6, (int) std::round (klatscher->load()));
    const float human = mensch->load();
    const float hand = ton->load();
    for (int k = 0; k < people; ++k)
        for (auto& c : claps)
            if (! c.active)
            {
                c.active = true;
                c.age = 0;
                c.delay = offset + (int) (rng.nextFloat() * human * 0.022 * sr) + (k == 0 ? 0 : (int) (rng.nextFloat() * 0.004 * sr));
                c.bursts = 2 + rng.nextInt (3);
                c.gain = accent * (0.7f + 0.3f * rng.nextFloat()) * (1.0f - human * 0.35f * rng.nextFloat()) / std::sqrt ((float) people);
                const float pan = people == 1 ? 0.0f : (rng.nextFloat() * 2.0f - 1.0f) * 0.8f;
                c.panL = std::sqrt (0.5f * (1.0f - pan)); c.panR = std::sqrt (0.5f * (1.0f + pan));
                c.filter.reset();
                c.filter.set ((900.0 + 1500.0 * hand) * (0.8 + 0.4 * rng.nextDouble()), sr, 1.6f);
                break;
            }
}

void PascherProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 0) buffer.clear();
    else if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    if (clapBuf.getNumSamples() < n) { clapBuf.setSize (2, n, false, false, true); wet.setSize (2, n, false, false, true); }
    clapBuf.clear();

    // Position im Takt: DAW-Transport, sonst frei im DAW-Tempo
    double bpm = 120.0, ppq = freePpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying() && pos->getPpqPosition().hasValue()) { ppq = *pos->getPpqPosition(); playing = true; }
        }
    const double beatsPerSample = bpm / 60.0 / sr;
    const double stepLen = rateBeats ((int) rate->load());
    const int len = juce::jlimit (2, numSteps, (int) std::round (laenge->load()));
    const float sw = swing->load();
    const float depth = tiefe->load();
    const float smooth = (float) std::exp (-1.0 / (juce::jmax (0.0005f, glaetten->load() * 0.001f) * sr));
    const float clapLevel = klatsch->load();
    const float smug = schmuggel->load();

    bool gateOn[numSteps], clapOn[numSteps];
    for (int s = 0; s < numSteps; ++s) { gateOn[s] = gates[s]->load() > 0.5f; clapOn[s] = clapSteps[s]->load() > 0.5f; }

    auto stepAt = [&] (double beat)
    {
        const double pos = beat / stepLen;
        const double pair = std::floor (pos / 2.0);
        const double t = pos - pair * 2.0;
        const long long idx = (long long) pair * 2 + (t >= 1.0 + sw ? 1 : 0);
        return (int) (((idx % len) + len) % len);
    };

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    mixSm.setTargetValue (mix->load());
    for (int i = 0; i < n; ++i)
    {
        const int step = stepAt (ppq + i * beatsPerSample);
        if (step != lastStep)
        {
            lastStep = step;
            // Schmuggel: ab und zu fällt ein Schlag weg oder ein Geisterschlag kommt dazu
            bool clap = clapOn[step];
            if (smug > 0.0f && rng.nextFloat() < smug * 0.35f) clap = ! clap;
            if (clap && clapLevel > 0.0f) triggerClaps (i, (step % 3 == 0 ? 1.0f : 0.75f) * (clap == clapOn[step] ? 1.0f : 0.5f));
        }
        const float target = gateOn[step] ? 1.0f : 1.0f - depth;
        gateGain = target + smooth * (gateGain - target);
        const float m = mixSm.getNextValue();
        const float g = 1.0f - m + m * gateGain;
        L[i] *= g; R[i] *= g;
    }
    currentStep = lastStep;
    if (! playing) freePpq = ppq + n * beatsPerSample;

    // Klatscher rendern: mehrere kurze Rauschstöße, dann ein kurzer Nachklang
    auto* cl = clapBuf.getWritePointer (0);
    auto* cr = clapBuf.getWritePointer (1);
    const int burstGap = (int) (0.009 * sr), burstLen = (int) (0.003 * sr);
    const float tailCoef = (float) std::exp (-1.0 / (0.045 * sr));
    for (auto& c : claps)
    {
        if (! c.active) continue;
        for (int i = 0; i < n; ++i)
        {
            if (c.delay > 0) { --c.delay; continue; }
            const int a = c.age++;
            float env;
            if (a < c.bursts * burstGap)
                env = (a % burstGap) < burstLen ? 1.0f : 0.15f;
            else
                env = std::pow (tailCoef, (float) (a - c.bursts * burstGap));
            const float x = c.filter.bandpass ((rng.nextFloat() * 2.0f - 1.0f)) * env * c.gain * 1.6f;
            cl[i] += x * c.panL; cr[i] += x * c.panR;
            if (a > c.bursts * burstGap + (int) (0.35 * sr)) { c.active = false; break; }
        }
    }

    // Wirtshaus: kleiner, warmer Raum nur für das Klatschen
    const float room = raum->load();
    tmk::FdnReverb::Settings rs;
    rs.sizeScale = 0.7f; rs.decay = 0.9f; rs.highRatio = 0.5f; rs.highCut = 4000.0f; rs.diffusion = 0.85f; rs.modDepth = 0.1f;
    reverb.update (rs);
    reverb.process (cl, cr, wet.getWritePointer (0), wet.getWritePointer (1), n);

    const float out = juce::Decibels::decibelsToGain (output->load());
    const auto* wl = wet.getReadPointer (0); const auto* wr = wet.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        L[i] = std::tanh ((L[i] + (cl[i] + wl[i] * room) * clapLevel) * out);
        R[i] = std::tanh ((R[i] + (cr[i] + wr[i] * room) * clapLevel) * out);
    }
}

juce::AudioProcessorEditor* PascherProcessor::createEditor() { return new PascherEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PascherProcessor(); }

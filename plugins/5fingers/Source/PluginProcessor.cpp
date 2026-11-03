#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace fingers;

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using PB = juce::AudioParameterBool;
    using PI = juce::AudioParameterInt; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<PB> (ID { "sync", 1 },        "Sync", true));
    p.push_back (std::make_unique<PC> (ID { "division", 1 },    "Takt", divisions, 3));
    p.push_back (std::make_unique<P>  (ID { "time", 1 },        u8 ("L\xc3\xa4nge (ms)"), tmk::skewed (50.0f, 4000.0f, 800.0f), 1000.0f));
    p.push_back (std::make_unique<P>  (ID { "feedback", 1 },    "Feedback", 0.0f, 0.95f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "ton", 1 },         "Ton (Hz)", tmk::skewed (800.0f, 18000.0f, 4000.0f), 9000.0f));
    p.push_back (std::make_unique<P>  (ID { "korngroesse", 1 }, u8 ("Korngr\xc3\xb6\xc3\x9f" "e (ms)"), tmk::skewed (10.0f, 400.0f, 80.0f), 90.0f));
    p.push_back (std::make_unique<PI> (ID { "tonhoehe", 1 },    u8 ("Tonh\xc3\xb6he (HT)"), -12, 12, 0));
    p.push_back (std::make_unique<P>  (ID { "streuung", 1 },    "Streuung", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<P>  (ID { "dichte", 1 },      "Dichte", 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "rueckwaerts", 1 }, u8 ("R\xc3\xbc" "ckw\xc3\xa4rts"), 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "breite", 1 },      "Breite", 0.0f, 1.0f, 0.8f));
    p.push_back (std::make_unique<P>  (ID { "mix", 1 },         "Mix", 0.0f, 1.0f, 0.35f));

    const float defZeit[]  = { 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };
    const float defPegel[] = { 0.8f, 0.7f, 0.6f, 0.5f, 0.45f };
    const float defPan[]   = { -0.8f, -0.4f, 0.0f, 0.4f, 0.8f };
    for (int f = 0; f < numFingers; ++f)
    {
        const auto n = juce::String (f + 1);
        p.push_back (std::make_unique<P> (ID { id ("zeit", f), 1 },  "Finger " + n + " Zeit", 0.02f, 1.0f, defZeit[f]));
        p.push_back (std::make_unique<P> (ID { id ("pegel", f), 1 }, "Finger " + n + " Pegel", 0.0f, 1.0f, defPegel[f]));
        p.push_back (std::make_unique<P> (ID { id ("pan", f), 1 },   "Finger " + n + " Pan", -1.0f, 1.0f, defPan[f]));
        p.push_back (std::make_unique<P> (ID { id ("korn", f), 1 },  "Finger " + n + u8 (" K\xc3\xb6rnung"), 0.0f, 1.0f, 0.0f));
    }
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Aussicht klar", {} },
        { "Wolkenmeer",          { { "korn3", 0.6f }, { "korn4", 0.8f }, { "korn5", 1.0f }, { "tonhoehe", 12 }, { "korngroesse", 160.0f },
                                   { "dichte", 0.8f }, { "feedback", 0.55f }, { "ton", 6000.0f }, { "mix", 0.45f } } },
        { "Eiskristalle",        { { "korn1", 0.7f }, { "korn2", 0.7f }, { "korn3", 0.7f }, { "korn4", 0.7f }, { "korn5", 0.7f },
                                   { "korngroesse", 25.0f }, { "streuung", 0.8f }, { "dichte", 0.35f }, { "tonhoehe", 12 }, { "division", 1 } } },
        { "F\xc3\xbcnf Gipfel",  { { "pegel1", 0.4f }, { "pegel2", 0.5f }, { "pegel3", 0.6f }, { "pegel4", 0.7f }, { "pegel5", 0.8f },
                                   { "feedback", 0.45f }, { "breite", 1.0f }, { "division", 4 } } },
        { "R\xc3\xbc" "ckw\xc3\xa4rts ins Tal", { { "rueckwaerts", 1.0f }, { "korn1", 0.8f }, { "korn2", 0.8f }, { "korn3", 0.8f }, { "korn4", 0.8f },
                                   { "korn5", 0.8f }, { "korngroesse", 250.0f }, { "dichte", 0.6f }, { "feedback", 0.4f }, { "mix", 0.45f } } },
        { "Tiefer Abgrund",      { { "tonhoehe", -12 }, { "korn5", 1.0f }, { "korn4", 0.6f }, { "feedback", 0.5f }, { "ton", 3000.0f },
                                   { "korngroesse", 140.0f } } },
        { "Rhythmus der Hand",   { { "division", 5 }, { "zeit1", 0.25f }, { "zeit2", 0.375f }, { "zeit3", 0.5f }, { "zeit4", 0.75f }, { "zeit5", 1.0f },
                                   { "feedback", 0.25f }, { "mix", 0.4f } } },
        { "Nebelschwaden",       { { "korn1", 0.5f }, { "korn2", 0.5f }, { "korn3", 0.5f }, { "korn4", 0.5f }, { "korn5", 0.5f },
                                   { "streuung", 1.0f }, { "dichte", 1.0f }, { "korngroesse", 300.0f }, { "feedback", 0.6f }, { "mix", 0.5f }, { "ton", 5000.0f } } },
        { "Slapback-Finger",     { { "sync", 0 }, { "time", 400.0f }, { "zeit1", 0.12f }, { "zeit2", 0.25f }, { "zeit3", 0.37f }, { "zeit4", 0.5f },
                                   { "zeit5", 0.62f }, { "feedback", 0.1f }, { "mix", 0.25f } } },
    };
}

FingersProcessor::FingersProcessor()
    : tmk::Processor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "5 FINGERS", factoryPresets())
{
    auto get = [this] (const juce::String& i) { return apvts.getRawParameterValue (i); };
    sync = get ("sync"); division = get ("division"); timeMs = get ("time"); feedback = get ("feedback"); ton = get ("ton");
    korngroesse = get ("korngroesse"); tonhoehe = get ("tonhoehe"); streuung = get ("streuung"); dichte = get ("dichte");
    rueckwaerts = get ("rueckwaerts"); breite = get ("breite"); mix = get ("mix");
    for (int f = 0; f < numFingers; ++f)
    {
        zeit[f] = get (id ("zeit", f)); pegel[f] = get (id ("pegel", f)); pan[f] = get (id ("pan", f)); korn[f] = get (id ("korn", f));
    }
}

bool FingersProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void FingersProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& b : buf) b.assign ((size_t) (sr * 5.5) + 16, 0.0f);
    w = 0;
    for (auto& f : fingerState) { for (auto& g : f.grains) g.active = false; f.untilNext = 0.0; }
    fbLp[0] = fbLp[1] = outLp[0] = outLp[1] = 0.0f;
    const double ms = sync->load() > 0.5f ? divisionBeats ((int) division->load()) * 60000.0 / readBpm() : (double) timeMs->load();
    baseSm.reset (sr, 0.3);
    baseSm.setCurrentAndTargetValue ((float) juce::jlimit (10.0, sr * 4.5, ms * 0.001 * sr));
    mixSm.reset (sr, 0.05);
    mixSm.setCurrentAndTargetValue (mix->load());
}

float FingersProcessor::readCh (int ch, double delay) const noexcept
{
    const int size = (int) buf[ch].size();
    double rp = (double) w - delay;
    while (rp < 0.0) rp += size;
    const int i0 = (int) rp;
    const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
    const float fr = (float) (rp - i0);
    return buf[ch][(size_t) i0] + fr * (buf[ch][(size_t) i1] - buf[ch][(size_t) i0]);
}

float FingersProcessor::readMono (double delay) const noexcept
{
    return 0.5f * (readCh (0, delay) + readCh (1, delay));
}

void FingersProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    const double ms = sync->load() > 0.5f ? divisionBeats ((int) division->load()) * 60000.0 / readBpm() : (double) timeMs->load();
    baseSm.setTargetValue ((float) juce::jlimit (10.0, sr * 4.5, ms * 0.001 * sr));
    baseMs = (float) juce::jmin (ms, 4500.0);
    mixSm.setTargetValue (mix->load());

    const float fb      = feedback->load();
    const float aTon    = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin ((double) ton->load(), 0.45 * sr) / sr);
    const int   grainLen = juce::jmax (16, (int) (korngroesse->load() * 0.001f * (float) sr));
    const double rate   = std::pow (2.0, std::round (tonhoehe->load()) / 12.0);
    const float scatter = streuung->load();
    const float overlap = 1.0f + 5.0f * dichte->load();
    const float revProb = rueckwaerts->load();
    const float width   = breite->load();
    const float grainNorm = 1.0f / std::sqrt (juce::jmax (1.0f, overlap * 0.375f));
    const double bufLimit = (double) buf[0].size() - 8.0;

    float z[numFingers], lvl[numFingers], pn[numFingers], kn[numFingers];
    for (int f = 0; f < numFingers; ++f) { z[f] = zeit[f]->load(); lvl[f] = pegel[f]->load(); pn[f] = pan[f]->load(); kn[f] = korn[f]->load(); }

    float peak[numFingers] {};
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float base = baseSm.getNextValue();
        float wetL = 0.0f, wetR = 0.0f, fbL = 0.0f, fbR = 0.0f;

        for (int f = 0; f < numFingers; ++f)
        {
            const double D = juce::jlimit (4.0, bufLimit, (double) (z[f] * base));
            auto& st = fingerState[f];

            // sauberes Echo, mit Balance
            float cl = readCh (0, D), cr = readCh (1, D);
            float gl = 0.0f, gr = 0.0f;

            // Granular-Wolke
            if (kn[f] > 0.001f)
            {
                st.untilNext -= 1.0;
                if (st.untilNext <= 0.0)
                {
                    st.untilNext += (double) grainLen / overlap * (1.0 + 0.6 * scatter * (rng.nextDouble() - 0.5));
                    for (auto& g : st.grains)
                        if (! g.active)
                        {
                            const bool rev = rng.nextFloat() < revProb;
                            g.step = rev ? 1.0 + rate : 1.0 - rate;           // Änderung der Verzögerung pro Sample
                            double d0 = D + scatter * grainLen * (rng.nextDouble() * 2.0 - 1.0);
                            if (g.step < 0.0) d0 = juce::jmax (d0, 4.0 - g.step * grainLen);
                            d0 = juce::jlimit (4.0, bufLimit - juce::jmax (0.0, g.step) * grainLen - 4.0, d0);
                            g.delay = d0;
                            g.length = grainLen; g.age = 0; g.active = true;
                            const float p = juce::jlimit (-1.0f, 1.0f, pn[f] + scatter * 0.6f * (rng.nextFloat() * 2.0f - 1.0f));
                            const float ang = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                            g.gainL = std::cos (ang) * 1.414f; g.gainR = std::sin (ang) * 1.414f;
                            break;
                        }
                }
                for (auto& g : st.grains)
                {
                    if (! g.active) continue;
                    const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) g.age / (float) g.length);
                    const float v = readMono (g.delay) * win;
                    gl += v * g.gainL; gr += v * g.gainR;
                    g.delay += g.step;
                    if (++g.age >= g.length) g.active = false;
                }
                gl *= grainNorm; gr *= grainNorm;
            }

            const float bl = juce::jmin (1.0f, 1.0f - pn[f]), br = juce::jmin (1.0f, 1.0f + pn[f]);
            const float k = kn[f];
            const float ol = cl * bl * (1.0f - k) + gl * k;
            const float orr = cr * br * (1.0f - k) + gr * k;
            if (f == numFingers - 1) { fbL = ol; fbR = orr; }        // Rückkopplung vom längsten Finger
            wetL += ol * lvl[f];
            wetR += orr * lvl[f];
            peak[f] = juce::jmax (peak[f], std::abs (ol * lvl[f]), std::abs (orr * lvl[f]));
        }

        // Rückkopplung: gedämpft und weich begrenzt
        fbLp[0] = fbL + aTon * (fbLp[0] - fbL);
        fbLp[1] = fbR + aTon * (fbLp[1] - fbR);
        const float inL = L[i], inR = R[i];
        buf[0][(size_t) w] = inL + std::tanh (fbLp[0] * fb);
        buf[1][(size_t) w] = inR + std::tanh (fbLp[1] * fb);
        if (++w >= (int) buf[0].size()) w = 0;

        // Ausgang: Ton, Breite, Mix
        outLp[0] = wetL + aTon * (outLp[0] - wetL);
        outLp[1] = wetR + aTon * (outLp[1] - wetR);
        const float mid = 0.5f * (outLp[0] + outLp[1]), side = 0.5f * (outLp[0] - outLp[1]) * width;
        const float m = mixSm.getNextValue();
        const float dryG = m < 0.5f ? 1.0f : 2.0f * (1.0f - m);
        const float wetG = (m < 0.5f ? 2.0f * m : 1.0f) * 0.7f;
        L[i] = inL * dryG + std::tanh ((mid + side) * wetG);
        R[i] = inR * dryG + std::tanh ((mid - side) * wetG);
    }

    for (int f = 0; f < numFingers; ++f)
    {
        fingerLevel[f] = juce::jmax (peak[f], fingerLevel[f].load() * 0.88f);
        int c = 0; for (auto& g : fingerState[f].grains) c += g.active ? 1 : 0;
        grainCount[f] = c;
    }
}

juce::AudioProcessorEditor* FingersProcessor::createEditor() { return new FingersEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FingersProcessor(); }

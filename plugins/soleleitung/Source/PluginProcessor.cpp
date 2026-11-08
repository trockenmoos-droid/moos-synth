#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using PB = juce::AudioParameterBool;
    using PI = juce::AudioParameterInt; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<PB> (ID { "sync", 1 },      "Sync", true));
    p.push_back (std::make_unique<PC> (ID { "division", 1 },  "Takt", sole::divisions, 5));
    p.push_back (std::make_unique<P>  (ID { "time", 1 },      "Zeit (ms)", tmk::skewed (1.0f, 2000.0f, 350.0f), 350.0f));
    p.push_back (std::make_unique<P>  (ID { "feedback", 1 },  "Feedback", 0.0f, 1.1f, 0.45f));
    p.push_back (std::make_unique<PI> (ID { "stations", 1 },  "Stationen", 1, 4, 1));
    p.push_back (std::make_unique<P>  (ID { "rohr", 1 },      "Rohr", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "rohrTon", 1 },   "Rohrton (Hz)", tmk::skewed (150.0f, 4000.0f, 800.0f), 800.0f));
    p.push_back (std::make_unique<P>  (ID { "salz", 1 },      "Salz", 0.0f, 1.0f, 0.2f));
    p.push_back (std::make_unique<P>  (ID { "stroemung", 1 }, u8 ("Str\xc3\xb6mung"), 0.0f, 1.0f, 0.2f));
    p.push_back (std::make_unique<P>  (ID { "ton", 1 },       "Ton (Hz)", tmk::skewed (800.0f, 18000.0f, 4000.0f), 6000.0f));
    p.push_back (std::make_unique<P>  (ID { "lowcut", 1 },    "Low Cut (Hz)", tmk::skewed (20.0f, 1000.0f, 200.0f), 120.0f));
    p.push_back (std::make_unique<P>  (ID { "breite", 1 },    "Breite", 0.0f, 1.0f, 0.6f));
    p.push_back (std::make_unique<P>  (ID { "duck", 1 },      "Ducking", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "mix", 1 },       "Mix", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<PB> (ID { "absperren", 1 }, "Absperren", false));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Hallstatt bis Ebensee", {} },
        { "Vier Stationen",        { { "stations", 4 }, { "division", 7 }, { "feedback", 0.35f }, { "breite", 0.8f } } },
        { "Salzkruste Dub",        { { "division", 3 }, { "feedback", 0.95f }, { "salz", 0.65f }, { "ton", 2600.0f }, { "lowcut", 220.0f },
                                     { "breite", 1.0f }, { "stroemung", 0.35f }, { "rohr", 0.45f }, { "mix", 0.4f } } },
        { "Holzrohr Slapback",     { { "sync", 0 }, { "time", 105.0f }, { "feedback", 0.12f }, { "rohr", 0.65f }, { "rohrTon", 1300.0f },
                                     { "breite", 0.2f }, { "mix", 0.3f } } },
        { "Undichte Stelle",       { { "stroemung", 0.85f }, { "ton", 3400.0f }, { "feedback", 0.62f }, { "rohr", 0.2f }, { "division", 4 } } },
        { "Sole Ping-Pong",        { { "division", 3 }, { "breite", 1.0f }, { "feedback", 0.5f }, { "rohr", 0.15f }, { "salz", 0.1f } } },
        { "Endlose Leitung",       { { "feedback", 1.04f }, { "salz", 0.45f }, { "stroemung", 0.4f }, { "ton", 3000.0f }, { "lowcut", 250.0f },
                                     { "division", 6 }, { "mix", 0.4f } } },
        { "Klarer Strom",          { { "rohr", 0.0f }, { "salz", 0.0f }, { "stroemung", 0.0f }, { "ton", 18000.0f }, { "lowcut", 20.0f },
                                     { "feedback", 0.4f }, { "breite", 0.5f } } },
        { "Stollen-Echo Vocals",   { { "division", 2 }, { "feedback", 0.3f }, { "duck", 0.6f }, { "stations", 2 }, { "lowcut", 300.0f },
                                     { "ton", 5000.0f }, { "mix", 0.3f } } },
    };
}

SoleleitungProcessor::SoleleitungProcessor()
    : tmk::Processor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "SOLELEITUNG", factoryPresets())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    sync = get ("sync"); division = get ("division"); timeMs = get ("time"); feedback = get ("feedback");
    stations = get ("stations"); rohr = get ("rohr"); rohrTon = get ("rohrTon"); salz = get ("salz");
    stroemung = get ("stroemung"); ton = get ("ton"); lowcut = get ("lowcut"); breite = get ("breite");
    duck = get ("duck"); mix = get ("mix"); absperren = get ("absperren");
}

bool SoleleitungProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void SoleleitungProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& b : buf) b.assign ((size_t) (sr * 4.2) + 16, 0.0f);   // 1/1 bei 60 BPM = 4 s
    w = 0;
    for (auto& c : cs) c = {};
    duckEnv = 0.0f;
    timeSm.reset (sr, 0.35);
    const double ms = sync->load() > 0.5f ? sole::divisionBeats ((int) division->load()) * 60000.0 / readBpm()
                                          : (double) timeMs->load();
    timeSm.setCurrentAndTargetValue ((float) juce::jlimit (8.0, sr * 4.0, ms * 0.001 * sr));
    mixSm.reset (sr, 0.05);
    mixSm.setCurrentAndTargetValue (mix->load());
}

float SoleleitungProcessor::read (int ch, float delay) const noexcept
{
    const int size = (int) buf[ch].size();
    float rp = (float) w - delay;
    while (rp < 0.0f) rp += (float) size;
    const int i0 = juce::jlimit (0, size - 1, (int) rp);
    const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
    const float fr = rp - (float) i0;
    return buf[ch][(size_t) i0] + fr * (buf[ch][(size_t) i1] - buf[ch][(size_t) i0]);
}

void SoleleitungProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 0) buffer.clear();
    else if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // Verzögerungszeit
    const double ms = sync->load() > 0.5f ? sole::divisionBeats ((int) division->load()) * 60000.0 / readBpm()
                                          : (double) timeMs->load();
    const float maxDelay = (float) buf[0].size() - 8.0f - (float) (0.03 * sr);
    timeSm.setTargetValue (juce::jlimit (8.0f, maxDelay, (float) (ms * 0.001 * sr)));
    delayMs = (float) ms;

    const bool  frz   = absperren->load() > 0.5f;
    const float fb    = frz ? 1.0f : feedback->load();
    const int   nSt   = juce::jlimit (1, 4, (int) std::round (stations->load()));
    const float pipe  = frz ? 0.0f : rohr->load();
    const float drive = 1.0f + 8.0f * salz->load();
    const float flow  = stroemung->load();
    // beim Absperren nur verlustfrei kreuzen (ganz oder gar nicht), damit nichts verloren geht
    const float wid   = frz ? (breite->load() > 0.5f ? 1.0f : 0.0f) : breite->load();
    const float duckAmt = duck->load();

    // Rohr-Resonanz (Bandpass, TPT-SVF)
    const float g  = (float) std::tan (juce::MathConstants<double>::pi * juce::jmin ((double) rohrTon->load(), 0.45 * sr) / sr);
    const float k  = 1.0f / 6.0f;
    const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float aHp = (float) std::exp (-juce::MathConstants<double>::twoPi * lowcut->load() / sr);
    const float aLp = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin ((double) ton->load(), 0.45 * sr) / sr);
    const float atk = (float) std::exp (-1.0 / (0.005 * sr)), rel = (float) std::exp (-1.0 / (0.3 * sr));

    auto loop = [&] (ChannelState& c, float x)
    {
        if (frz) return x;
        // Rohr: Resonanzband hervorheben (normierter Bandpass, Spitzenverstärkung 1 -> stabil)
        const float v3 = x - c.ic2;
        const float v1 = a1 * c.ic1 + a2 * v3;
        const float v2 = c.ic2 + a2 * c.ic1 + a3 * v3;
        c.ic1 = 2.0f * v1 - c.ic1; c.ic2 = 2.0f * v2 - c.ic2;
        float y = x * (1.0f - 0.7f * pipe) + k * v1 * pipe * 0.7f;
        // Low Cut + Ton
        c.hp = y + aHp * (c.hp - y); y -= c.hp;
        c.lp = y + aLp * (c.lp - y); y = c.lp;
        // Salz: weiche Sättigung, Kleinsignal-Verstärkung 1 -> bleibt stabil
        return std::tanh (drive * y) / drive;
    };

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    mixSm.setTargetValue (mix->load());
    float stPeak[4] {};
    const double wowInc = 0.55 / sr, flutInc = 5.3 / sr;

    for (int i = 0; i < n; ++i)
    {
        const float D = timeSm.getNextValue();

        // Strömung: langsames Wow, schnelles Flutter, zufällige Drift
        wowPhase += wowInc; if (wowPhase >= 1.0) wowPhase -= 1.0;
        flutterPhase += flutInc; if (flutterPhase >= 1.0) flutterPhase -= 1.0;
        if (rng.nextFloat() < 1.0f / (float) (sr * 0.7)) driftTarget = rng.nextFloat() * 2.0f - 1.0f;
        drift += (driftTarget - drift) * 0.00005f;
        const float mod = flow * (0.0045f * (float) std::sin (wowPhase * juce::MathConstants<double>::twoPi)
                                + 0.0012f * (float) std::sin (flutterPhase * juce::MathConstants<double>::twoPi)
                                + 0.003f * drift);
        const float dL = D * (1.0f + mod), dR = D * (1.0f + mod * 0.85f) + flow * 0.0006f * D;

        // Stationen: Abgriffe bei k/N der Laufzeit
        float outL = 0.0f, outR = 0.0f;
        for (int s = 1; s <= nSt; ++s)
        {
            const float frac = (float) s / (float) nSt;
            const float gain = std::pow (0.82f, (float) (s - 1)) * (nSt > 1 ? 0.85f : 1.0f);
            const float vl = read (0, dL * frac) * gain, vr = read (1, dR * frac) * gain;
            const float pan = nSt > 1 ? ((s % 2) ? -0.35f : 0.35f) * wid : 0.0f;
            outL += vl * (1.0f - pan); outR += vr * (1.0f + pan);
            stPeak[s - 1] = juce::jmax (stPeak[s - 1], std::abs (vl), std::abs (vr));
        }

        // Rückkopplung vom Ende der Leitung, mit Kreuzung für Ping-Pong
        const float endL = read (0, dL), endR = read (1, dR);
        const float fL = loop (cs[0], endL * (1.0f - wid) + endR * wid) * fb;
        const float fR = loop (cs[1], endR * (1.0f - wid) + endL * wid) * fb;

        const float inL = L[i], inR = R[i];
        const float mono = 0.5f * (inL + inR);
        const float sendL = frz ? 0.0f : inL * (1.0f - wid) + mono * wid;
        const float sendR = frz ? 0.0f : inR * (1.0f - wid);
        buf[0][(size_t) w] = juce::jlimit (-4.0f, 4.0f, sendL + fL);
        buf[1][(size_t) w] = juce::jlimit (-4.0f, 4.0f, sendR + fR);
        if (++w >= (int) buf[0].size()) w = 0;

        const float lvl = juce::jmax (std::abs (inL), std::abs (inR));
        duckEnv = lvl > duckEnv ? lvl + atk * (duckEnv - lvl) : lvl + rel * (duckEnv - lvl);
        const float duckGain = 1.0f - duckAmt * juce::jmin (1.0f, duckEnv * 4.0f);

        const float m = mixSm.getNextValue();
        const float dryG = m < 0.5f ? 1.0f : 2.0f * (1.0f - m);
        const float wetG = (m < 0.5f ? 2.0f * m : 1.0f) * duckGain;
        L[i] = inL * dryG + std::tanh (outL * wetG);
        R[i] = inR * dryG + std::tanh (outR * wetG);
    }

    for (int s = 0; s < 4; ++s)
        stationLevel[s] = s < nSt ? juce::jmax (stPeak[s], stationLevel[s].load() * 0.88f) : 0.0f;
}

juce::AudioProcessorEditor* SoleleitungProcessor::createEditor() { return new SoleleitungEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SoleleitungProcessor(); }

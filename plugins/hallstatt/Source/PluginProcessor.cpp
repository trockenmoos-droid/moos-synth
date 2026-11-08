#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace hallstatt;

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using PB = juce::AudioParameterBool;
    using ID = juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<PC> (ID { "room", 1 },     "Raum", roomNames, 0));
    p.push_back (std::make_unique<P>  (ID { "size", 1 },     juce::String::fromUTF8 ("Gr\xc3\xb6\xc3\x9f" "e"), 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "decay", 1 },    "Nachhall (s)", tmk::skewed (0.3f, 30.0f, 3.0f), 3.5f));
    p.push_back (std::make_unique<P>  (ID { "predelay", 1 }, "Predelay (ms)", tmk::skewed (0.0f, 250.0f, 40.0f), 15.0f));
    p.push_back (std::make_unique<P>  (ID { "damping", 1 },  juce::String::fromUTF8 ("D\xc3\xa4mpfung"), 0.0f, 1.0f, 0.55f));
    p.push_back (std::make_unique<P>  (ID { "bass", 1 },     "Bass-Nachhall", 0.5f, 2.0f, 1.1f));
    p.push_back (std::make_unique<P>  (ID { "mod", 1 },      "Modulation", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "early", 1 },    juce::String::fromUTF8 ("Fr\xc3\xbch" "e Reflexionen"), 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "width", 1 },    "Breite", 0.0f, 1.0f, 1.0f));
    p.push_back (std::make_unique<P>  (ID { "lowcut", 1 },   "Low Cut (Hz)", tmk::skewed (20.0f, 800.0f, 150.0f), 80.0f));
    p.push_back (std::make_unique<P>  (ID { "highcut", 1 },  "High Cut (Hz)", tmk::skewed (1000.0f, 20000.0f, 6000.0f), 12000.0f));
    p.push_back (std::make_unique<P>  (ID { "duck", 1 },     "Ducking", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "mix", 1 },      "Mix", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<PB> (ID { "freeze", 1 },   "Freeze", false));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Salzbergwerk", {} },
        { "Beinhaus Gesang",        { { "room", 1 }, { "decay", 1.2f }, { "predelay", 8.0f }, { "damping", 0.35f }, { "early", 0.6f }, { "mix", 0.25f } } },
        { "Pfarrkirche Orgel",      { { "room", 2 }, { "decay", 6.5f }, { "predelay", 32.0f }, { "bass", 1.35f }, { "damping", 0.5f }, { "size", 0.7f }, { "mix", 0.42f } } },
        { "Abend am See",           { { "room", 3 }, { "decay", 7.5f }, { "predelay", 45.0f }, { "early", 0.75f }, { "damping", 0.7f }, { "size", 0.6f }, { "mix", 0.45f }, { "lowcut", 120.0f } } },
        { "Eish\xc3\xb6hle",        { { "room", 4 }, { "decay", 13.0f }, { "predelay", 25.0f }, { "mod", 0.2f }, { "damping", 0.12f }, { "bass", 0.8f }, { "size", 0.8f }, { "mix", 0.5f } } },
        { "Unendliche Kirche",      { { "room", 2 }, { "decay", 30.0f }, { "predelay", 60.0f }, { "mod", 0.55f }, { "damping", 0.6f }, { "size", 1.0f }, { "mix", 0.7f }, { "lowcut", 200.0f } } },
        { "Stollen-Snare",          { { "room", 0 }, { "decay", 1.6f }, { "size", 0.3f }, { "early", 0.8f }, { "duck", 0.5f }, { "mix", 0.3f }, { "lowcut", 180.0f } } },
        { "Drums gedrosselt",       { { "room", 2 }, { "decay", 2.2f }, { "size", 0.4f }, { "duck", 0.65f }, { "lowcut", 250.0f }, { "highcut", 8000.0f }, { "mix", 0.35f } } },
        { "Shimmernde Eisfl\xc3\xa4" "che", { { "room", 4 }, { "decay", 22.0f }, { "mod", 0.8f }, { "damping", 0.05f }, { "width", 1.0f }, { "early", 0.2f }, { "mix", 0.6f } } },
    };
}

HallstattProcessor::HallstattProcessor()
    : tmk::Processor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                     createLayout(), "HALLSTATT", factoryPresets())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    room = get ("room"); size = get ("size"); decay = get ("decay"); predelay = get ("predelay");
    damping = get ("damping"); bass = get ("bass"); mod = get ("mod"); early = get ("early");
    width = get ("width"); lowcut = get ("lowcut"); highcut = get ("highcut"); mix = get ("mix");
    freeze = get ("freeze"); duck = get ("duck");
}

bool HallstattProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    const auto in  = l.getMainInputChannelSet();
    return out == juce::AudioChannelSet::stereo() && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void HallstattProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    fdn.prepare (sr);
    for (auto& b : pre) b.assign ((size_t) (sr * 1.7) + 8, 0.0f);
    preW = 0;
    erLp[0] = erLp[1] = lpS[0] = lpS[1] = 0.0f;
    for (auto& h : hpS) h[0] = h[1] = 0.0f;
    duckEnv = 0.0f;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 1 };
    for (auto& c : colour) { c.prepare (spec); c.reset(); }
    lastRoom = -1;
    wet.setSize (2, juce::jmax (samplesPerBlock, maxChunk));
    fdnIn.setSize (2, juce::jmax (samplesPerBlock, maxChunk));
    fdnOut.setSize (2, juce::jmax (samplesPerBlock, maxChunk));
    mixSm.reset (sr, 0.05);
    mixSm.setCurrentAndTargetValue (mix->load());
}

void HallstattProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (wet.getNumSamples() < n) { wet.setSize (2, n, false, false, true); fdnIn.setSize (2, n, false, false, true); fdnOut.setSize (2, n, false, false, true); }
    if (getTotalNumInputChannels() == 0) buffer.clear();
    else if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    const int r = juce::jlimit (0, (int) rooms().size() - 1, (int) room->load());
    const auto& rm = rooms()[(size_t) r];
    if (r != lastRoom)
    {
        lastRoom = r;
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, rm.colourFreq, rm.colourQ,
                                                                          juce::Decibels::decibelsToGain (rm.colourGainDb));
        for (auto& c : colour) c.coefficients = coeffs;
    }

    const float sz   = size->load();
    const float damp = damping->load();
    const bool  frz  = freeze->load() > 0.5f;

    tmk::FdnReverb::Settings s;
    s.sizeScale = rm.sizeScale * (0.5f + sz);
    s.decay     = decay->load();
    s.bassRatio = bass->load();
    s.highRatio = 1.0f - 0.85f * damp;
    s.highCut   = 9000.0f * (1.0f - 0.75f * damp) + 800.0f;
    s.modDepth  = mod->load();
    s.modRate   = rm.modRate * (0.5f + mod->load());
    s.diffusion = rm.diffusion;
    s.width     = width->load();
    s.freeze    = frz;
    fdn.update (s);

    const int   preSize  = (int) pre[0].size();
    const float preSamps = predelay->load() * 0.001f * (float) sr;
    const float erScale  = (0.5f + sz) * 0.001f * (float) sr;
    const float erGain   = frz ? 0.0f : early->load() * 0.9f;
    const float aEr  = (float) std::exp (-juce::MathConstants<double>::twoPi * rm.erLowpass / sr);
    const float aHp  = (float) std::exp (-juce::MathConstants<double>::twoPi * lowcut->load() / sr);
    const float aLp  = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin ((double) highcut->load(), 0.45 * sr) / sr);
    const float duckAmt = duck->load();
    const float atk = (float) std::exp (-1.0 / (0.005 * sr)), rel = (float) std::exp (-1.0 / (0.25 * sr));

    auto readPre = [&] (int ch, float delay)
    {
        float rp = (float) preW - delay;
        while (rp < 0.0f) rp += (float) preSize;
        const int i0 = juce::jlimit (0, preSize - 1, (int) rp);
        const int i1 = (i0 + 1) % preSize;
        const float fr = rp - (float) i0;
        return pre[ch][(size_t) i0] + fr * (pre[ch][(size_t) i1] - pre[ch][(size_t) i0]);
    };

    const auto* inL = buffer.getReadPointer (0);
    const auto* inR = buffer.getReadPointer (1);
    auto* fl = fdnIn.getWritePointer (0); auto* fr = fdnIn.getWritePointer (1);
    auto* wl = wet.getWritePointer (0);   auto* wr = wet.getWritePointer (1);

    // 1) Predelay + frühe Reflexionen
    float inPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        pre[0][(size_t) preW] = inL[i];
        pre[1][(size_t) preW] = inR[i];
        inPeak = juce::jmax (inPeak, std::abs (inL[i]), std::abs (inR[i]));

        fl[i] = readPre (0, preSamps + 1.0f);
        fr[i] = readPre (1, preSamps + 1.0f);

        float el = 0.0f, er = 0.0f;
        for (const auto& t : rm.taps)
        {
            const float d = preSamps + 1.0f + t.ms * erScale;
            const float m = 0.5f * (readPre (0, d) + readPre (1, d)) * t.gain;
            el += m * (1.0f - t.pan) * 0.5f;
            er += m * (1.0f + t.pan) * 0.5f;
        }
        erLp[0] = el + aEr * (erLp[0] - el);
        erLp[1] = er + aEr * (erLp[1] - er);
        wl[i] = erLp[0] * erGain;
        wr[i] = erLp[1] * erGain;

        if (++preW >= preSize) preW = 0;
    }

    // 2) Später Nachhall (FDN); Eingang bekommt auch die frühen Reflexionen -> dichterer Übergang
    for (int i = 0; i < n; ++i) { fl[i] += wl[i] * 0.5f; fr[i] += wr[i] * 0.5f; }
    auto* tl = fdnOut.getWritePointer (0);
    auto* tr = fdnOut.getWritePointer (1);
    fdn.process (fl, fr, tl, tr, n);

    // 3) Klangfarbe, Low/High Cut, Ducking, Mix
    float wetPeak = 0.0f;
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    mixSm.setTargetValue (mix->load());
    for (int i = 0; i < n; ++i)
    {
        float w[2] = { wl[i] + tl[i], wr[i] + tr[i] };
        for (int ch = 0; ch < 2; ++ch)
        {
            float v = colour[ch].processSample (w[ch]);
            // 12 dB Low Cut (2x 1-Pol)
            hpS[ch][0] = v + aHp * (hpS[ch][0] - v);           v -= hpS[ch][0];
            hpS[ch][1] = v + aHp * (hpS[ch][1] - v);           v -= hpS[ch][1];
            lpS[ch]    = v + aLp * (lpS[ch] - v);              v = lpS[ch];
            w[ch] = v;
        }

        const float lvl = juce::jmax (std::abs (inL[i]), std::abs (inR[i]));
        duckEnv = lvl > duckEnv ? lvl + atk * (duckEnv - lvl) : lvl + rel * (duckEnv - lvl);
        const float duckGain = 1.0f - duckAmt * juce::jmin (1.0f, duckEnv * 4.0f);

        const float m = mixSm.getNextValue();
        const float dryG = m < 0.5f ? 1.0f : 2.0f * (1.0f - m);
        const float wetG = (m < 0.5f ? 2.0f * m : 1.0f) * duckGain;
        const float dl = inL[i], dr = inR[i];
        outL[i] = dl * dryG + w[0] * wetG;
        outR[i] = dr * dryG + w[1] * wetG;
        wetPeak = juce::jmax (wetPeak, std::abs (w[0]), std::abs (w[1]));
    }

    inputLevel = juce::jmax (inPeak, inputLevel.load() * 0.85f);
    wetLevel   = juce::jmax (wetPeak, wetLevel.load() * 0.9f);
}

juce::AudioProcessorEditor* HallstattProcessor::createEditor() { return new HallstattEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HallstattProcessor(); }

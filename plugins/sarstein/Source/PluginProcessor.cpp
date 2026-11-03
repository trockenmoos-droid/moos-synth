#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using PB = juce::AudioParameterBool;
    using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<PC> (ID { "stein", 1 },       "Gestein", sarstein::stones, 0));
    p.push_back (std::make_unique<P>  (ID { "drive", 1 },       "Drive (dB)", 0.0f, 48.0f, 12.0f));
    p.push_back (std::make_unique<P>  (ID { "geroell", 1 },     u8 ("Ger\xc3\xb6ll"), 0.0f, 1.0f, 0.15f));
    p.push_back (std::make_unique<P>  (ID { "steinschlag", 1 }, "Steinschlag", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "tiefe", 1 },       "Tiefe (Hz)", tmk::skewed (20.0f, 800.0f, 120.0f), 40.0f));
    p.push_back (std::make_unique<P>  (ID { "ton", 1 },         "Ton", -1.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "highcut", 1 },     "High Cut (Hz)", tmk::skewed (1000.0f, 20000.0f, 6000.0f), 14000.0f));
    p.push_back (std::make_unique<P>  (ID { "mix", 1 },         "Mix", 0.0f, 1.0f, 1.0f));
    p.push_back (std::make_unique<P>  (ID { "output", 1 },      "Ausgang (dB)", -24.0f, 12.0f, 0.0f));
    p.push_back (std::make_unique<PB> (ID { "autogain", 1 },    "Auto-Pegel", true));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Kalk sanft", {} },
        { "Granitblock",       { { "stein", 1 }, { "drive", 30.0f }, { "ton", -0.2f }, { "tiefe", 80.0f } } },
        { "Ger\xc3\xb6llhalde", { { "stein", 2 }, { "drive", 24.0f }, { "geroell", 0.7f }, { "highcut", 9000.0f } } },
        { "Bergkristall",      { { "stein", 3 }, { "drive", 20.0f }, { "highcut", 9000.0f }, { "mix", 0.8f } } },
        { "Steinschlag",       { { "stein", 0 }, { "drive", 18.0f }, { "steinschlag", 0.7f }, { "highcut", 11000.0f } } },
        { "Sarstein-Wand",     { { "stein", 1 }, { "drive", 40.0f }, { "tiefe", 150.0f }, { "ton", 0.3f }, { "highcut", 7000.0f } } },
        { "Sanfter Bach",      { { "stein", 0 }, { "drive", 6.0f }, { "mix", 0.5f }, { "geroell", 0.25f } } },
        { "Drum-Bus Fels",     { { "stein", 2 }, { "drive", 14.0f }, { "geroell", 0.3f }, { "mix", 0.6f }, { "ton", 0.15f } } },
        { "Kristallh\xc3\xb6hle", { { "stein", 3 }, { "drive", 34.0f }, { "steinschlag", 0.25f }, { "ton", 0.4f }, { "mix", 0.55f } } },
    };
}

SarsteinProcessor::SarsteinProcessor()
    : tmk::Processor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "SARSTEIN", factoryPresets())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    stein = get ("stein"); drive = get ("drive"); geroell = get ("geroell"); steinschlag = get ("steinschlag");
    tiefe = get ("tiefe"); ton = get ("ton"); highcut = get ("highcut"); mix = get ("mix"); output = get ("output");
    autogain = get ("autogain");
}

bool SarsteinProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void SarsteinProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) juce::jmax (samplesPerBlock, maxChunk));
    setLatencySamples ((int) std::round (oversampling->getLatencyInSamples()));
    dry.setSize (2, juce::jmax (samplesPerBlock, maxChunk));
    driveCurve.assign ((size_t) juce::jmax (samplesPerBlock, maxChunk), 1.0f);
    for (auto& r : dryRing) r.assign (256, 0.0f);
    dryW = 0;
    for (int c = 0; c < 2; ++c) hp1[c] = hp2[c] = dc[c] = dcIn[c] = lp[c] = tiltLp[c] = hold[c] = 0.0f;
    holdCount = 0;
    driveSm.reset (sr, 0.03); driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (drive->load()));
    outSm.reset (sr, 0.03);   outSm.setCurrentAndTargetValue (1.0f);
    mixSm.reset (sr, 0.03);   mixSm.setCurrentAndTargetValue (mix->load());
}

void SarsteinProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    for (int c = 0; c < 2; ++c) dry.copyFrom (c, 0, buffer, c, 0, n);

    const int   st    = juce::jlimit (0, 3, (int) stein->load());
    const float dB    = drive->load();
    const float bias  = geroell->load() * 0.6f;
    const float crush = steinschlag->load();
    const float aHp   = (float) std::exp (-juce::MathConstants<double>::twoPi * tiefe->load() / sr);
    const float aLp   = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin ((double) highcut->load(), 0.45 * sr) / sr);
    const float aTilt = (float) std::exp (-juce::MathConstants<double>::twoPi * 900.0 / sr);
    const float tilt  = ton->load();
    const float lowG  = juce::Decibels::decibelsToGain (-6.0f * tilt), highG = juce::Decibels::decibelsToGain (6.0f * tilt);
    const float aDc   = (float) std::exp (-juce::MathConstants<double>::twoPi * 12.0 / sr);
    const float biasOut = sarstein::shape (st, bias);

    // Auto-Pegel: grobe Kompensation des Drives, damit lauter nicht automatisch "besser" klingt
    float makeup = juce::Decibels::decibelsToGain (output->load());
    if (autogain->load() > 0.5f)
    {
        // Verhältnis Ausgang/Eingang bei einem typischen Pegel (-12 dBFS), Bergkristall über mehrere Punkte gemittelt
        float acc = 0.0f;
        for (float a0 : { 0.12f, 0.25f, 0.4f })
            acc += std::abs (sarstein::shape (st, a0 * juce::Decibels::decibelsToGain (dB) + bias) - biasOut) / a0;
        makeup *= juce::jlimit (0.05f, 4.0f, 3.0f / juce::jmax (0.05f, acc));
    }
    driveSm.setTargetValue (juce::Decibels::decibelsToGain (dB));
    outSm.setTargetValue (makeup);
    mixSm.setTargetValue (mix->load());

    float inPeak = buffer.getMagnitude (0, n);
    inPeak = juce::jmax (inPeak, buffer.getMagnitude (1, n));

    // 1) Tiefe: Hochpass vor der Verzerrung (2x 1-Pol), dann Drive
    if ((int) driveCurve.size() < n) driveCurve.resize ((size_t) n);
    for (int i = 0; i < n; ++i) driveCurve[(size_t) i] = driveSm.getNextValue();
    for (int c = 0; c < 2; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            hp1[c] = x + aHp * (hp1[c] - x); x -= hp1[c];
            hp2[c] = x + aHp * (hp2[c] - x); x -= hp2[c];
            d[i] = x * driveCurve[(size_t) i];
        }
    }

    // 2) Kennlinie mit 4-fachem Oversampling
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = oversampling->processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                d[i] = sarstein::shape (st, d[i] + bias) - biasOut;
        }
        oversampling->processSamplesDown (block);
    }

    // 3) Steinschlag, DC-Filter, Ton (Tilt), High Cut, Pegel, Mix
    const float bits  = 16.0f - 13.0f * crush;
    const float steps = std::pow (2.0f, bits - 1.0f);
    const int   holdN = 1 + (int) (crush * crush * 23.0f);
    const int   lat   = getLatencySamples();
    float outPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const bool takeNew = (holdCount++ % holdN) == 0;
        const float g = outSm.getNextValue();
        const float m = mixSm.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            float x = d[i];
            if (crush > 0.001f)
            {
                if (takeNew) hold[c] = std::round (x * steps) / steps;
                x = hold[c];
            }
            // DC entfernen (Geröll erzeugt Gleichanteil)
            const float y = x - dcIn[c] + aDc * dc[c];
            dcIn[c] = x; dc[c] = y; x = y;
            // Tilt um 900 Hz
            tiltLp[c] = x + aTilt * (tiltLp[c] - x);
            x = tiltLp[c] * lowG + (x - tiltLp[c]) * highG;
            lp[c] = x + aLp * (lp[c] - x);
            x = lp[c] * g;
            // Dry um die Oversampling-Latenz verzögern, damit Mix phasenrichtig ist
            dryRing[c][(size_t) dryW] = dry.getSample (c, i);
            const float dr = dryRing[c][(size_t) ((dryW - lat + 256) & 255)];
            d[i] = x * m + dr * (1.0f - m);
            outPeak = juce::jmax (outPeak, std::abs (d[i]));
        }
        dryW = (dryW + 1) & 255;
    }

    inLevel  = juce::jmax (inPeak,  inLevel.load() * 0.85f);
    outLevel = juce::jmax (outPeak, outLevel.load() * 0.85f);
}

juce::AudioProcessorEditor* SarsteinProcessor::createEditor() { return new SarsteinEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SarsteinProcessor(); }

#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<P>  (ID { "carnyx", 1 },    "Carnyx", 0.0f, 1.0f, 0.8f));
    p.push_back (std::make_unique<P>  (ID { "growl", 1 },     "Growl", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<P>  (ID { "zunge", 1 },     "Zunge", 0.0f, 1.0f, 0.2f));
    p.push_back (std::make_unique<P>  (ID { "anblasen", 1 },  "Anblasen", 0.0f, 1.0f, 0.4f));
    p.push_back (std::make_unique<P>  (ID { "hell", 1 },      "Helligkeit", 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "attack", 1 },    "Attack (s)", tmk::skewed (0.005f, 3.0f, 0.2f), 0.08f));
    p.push_back (std::make_unique<P>  (ID { "release", 1 },   "Release (s)", tmk::skewed (0.02f, 5.0f, 0.5f), 0.4f));
    p.push_back (std::make_unique<P>  (ID { "bordun", 1 },    "Bordun", 0.0f, 1.0f, 0.4f));
    p.push_back (std::make_unique<PC> (ID { "bordunTon", 1 }, "Bordun-Ton", kelten::keys, 2));
    p.push_back (std::make_unique<P>  (ID { "quinte", 1 },    "Quinte", 0.0f, 1.0f, 0.6f));
    p.push_back (std::make_unique<P>  (ID { "erde", 1 },      "Erde", 0.0f, 1.0f, 0.2f));
    p.push_back (std::make_unique<P>  (ID { "rost", 1 },      "Rost", 0.0f, 1.0f, 0.15f));
    p.push_back (std::make_unique<P>  (ID { "alter", 1 },     "Alter", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "hall", 1 },      u8 ("H\xc3\xb6hle"), 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<P>  (ID { "volume", 1 },    u8 ("Lautst\xc3\xa4rke (dB)"), -48.0f, 6.0f, -6.0f));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Carnyx am Ritualplatz", {} },
        { "Kriegshorn",         { { "growl", 0.8f }, { "zunge", 0.6f }, { "hell", 0.8f }, { "anblasen", 0.7f }, { "bordun", 0.0f }, { "hall", 0.5f } } },
        { "Dunkler Bordun",     { { "carnyx", 0.35f }, { "hell", 0.25f }, { "bordun", 0.8f }, { "quinte", 0.8f }, { "attack", 0.8f }, { "release", 2.0f },
                                  { "erde", 0.35f }, { "alter", 0.5f } } },
        { "Fund aus dem Salzberg", { { "rost", 0.6f }, { "alter", 0.8f }, { "erde", 0.5f }, { "growl", 0.5f }, { "hall", 0.5f } } },
        { "Sanftes Horn",       { { "growl", 0.05f }, { "zunge", 0.0f }, { "hell", 0.35f }, { "anblasen", 0.2f }, { "bordun", 0.2f }, { "attack", 0.2f },
                                  { "erde", 0.0f }, { "rost", 0.0f }, { "alter", 0.1f } } },
        { "Grabhügel",          { { "carnyx", 0.5f }, { "bordun", 0.7f }, { "bordunTon", 9 }, { "erde", 0.6f }, { "alter", 0.7f }, { "hall", 0.7f },
                                  { "attack", 1.5f }, { "release", 3.0f }, { "hell", 0.3f } } },
        { "Eisenzeit-Bass",     { { "carnyx", 1.0f }, { "hell", 0.4f }, { "growl", 0.6f }, { "attack", 0.01f }, { "release", 0.15f }, { "bordun", 0.0f },
                                  { "rost", 0.35f }, { "hall", 0.1f } } },
    };
}

KeltenProcessor::KeltenProcessor()
    : tmk::Processor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "KELTEN", factoryPresets())
{
    auto get = [this] (const char* i) { return apvts.getRawParameterValue (i); };
    carnyx = get ("carnyx"); growl = get ("growl"); zunge = get ("zunge"); anblasen = get ("anblasen"); hell = get ("hell");
    attack = get ("attack"); release = get ("release"); bordun = get ("bordun"); bordunTon = get ("bordunTon"); quinte = get ("quinte");
    erde = get ("erde"); rost = get ("rost"); alter = get ("alter"); hall = get ("hall"); volume = get ("volume");
}

bool KeltenProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void KeltenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    kp.sr = sampleRate;
    juce::uint32 seed = 400;
    for (auto& v : voices.voices) v.setup (sampleRate, &kp, seed++);
    voices.reset();
    keyboardState.reset();
    reverb.prepare (sampleRate);
    wet.setSize (2, juce::jmax (samplesPerBlock, maxChunk));
    for (auto& b : wowBuf) b.assign ((size_t) (sampleRate * 0.05) + 8, 0.0f);
    wowW = 0; wowPhase = 0.0;
    for (int c = 0; c < 2; ++c) { hold[c] = ageLp[c] = 0.0f; droneLp[c].reset(); dronePhase[c] = 0.0; }
    droneEnv = 0.0f; rumble = 0.0f; holdCount = 0;
    gainSm.reset (sampleRate, 0.05);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
}

void KeltenProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const double sr = kp.sr;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (wet.getNumSamples() < n) wet.setSize (2, n, false, false, true);

    kp.carnyx = carnyx->load(); kp.growl = growl->load(); kp.zunge = zunge->load(); kp.anblasen = anblasen->load(); kp.hell = hell->load();
    const juce::ADSR::Parameters ep { attack->load(), 0.2f, 0.85f, release->load() };
    for (auto& v : voices.voices) v.env.setParameters (ep);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    voices.process (L, R, n, midi);

    // Bordun: Grundton (Oktave 2) + Quinte, Rohrblatt-artig; setzt ein, solange gespielt wird
    const float bord = bordun->load();
    const bool playing = voices.activeCount() > 0;
    const double rootNote = 36.0 + juce::jlimit (0, 11, (int) bordunTon->load());
    const float q = quinte->load();
    const float envCoef = (float) (1.0 - std::exp (-1.0 / ((playing ? 0.8 : 2.5) * sr)));
    if (bord > 0.0f || droneEnv > 1.0e-4f)
    {
        for (int c = 0; c < 2; ++c) droneLp[c].set (1400.0, sr, 0.8f);
        for (int i = 0; i < n; ++i)
        {
            droneEnv += ((playing ? 1.0f : 0.0f) - droneEnv) * envCoef;
            if (rng.nextFloat() < 1.0f / (float) sr) droneDrift = (rng.nextFloat() - 0.5f) * 0.1f;
            float s[2];
            for (int k = 0; k < 2; ++k)
            {
                const double f = tmk::VoiceBase::hz (rootNote + (k == 1 ? 7.0 : 0.0) + droneDrift);
                const double inc = f / sr;
                dronePhase[k] += inc; if (dronePhase[k] >= 1.0) dronePhase[k] -= 1.0;
                // schmaler Puls = Differenz zweier Sägezähne
                double p2 = dronePhase[k] + 0.82; if (p2 >= 1.0) p2 -= 1.0;
                s[k] = tmk::dsp::sawBlep (dronePhase[k], inc) - tmk::dsp::sawBlep (p2, inc);
            }
            const float a = droneLp[0].lowpass (s[0]) * 0.12f * bord * droneEnv;
            const float b = droneLp[1].lowpass (s[1]) * 0.1f * bord * q * droneEnv;
            L[i] += a * 0.8f + b * 0.5f;
            R[i] += a * 0.5f + b * 0.8f;
        }
    }

    // Eisenzeit: Alter (Bandbegrenzung + Leiern), Rost (Bits/Rate), Erde (Rumpeln + Knistern)
    const float age = alter->load(), rust = rost->load(), earth = erde->load();
    const float aAge = tmk::dsp::onePoleCoef (16000.0 - 13000.0 * age, sr);
    const float steps = std::pow (2.0f, 15.0f - 10.0f * rust);
    const int holdN = 1 + (int) (rust * rust * 8.0f);
    const int wsz = (int) wowBuf[0].size();
    for (int i = 0; i < n; ++i)
    {
        wowPhase += 0.45 / sr; if (wowPhase >= 1.0) wowPhase -= 1.0;
        const float d = 0.012f * (float) sr * (0.5f + 0.4f * age * tmk::dsp::sine (wowPhase));
        const bool take = (holdCount++ % holdN) == 0;
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
        {
            wowBuf[c][(size_t) wowW] = ch[c][i];
            float rp = (float) wowW - d;
            while (rp < 0.0f) rp += (float) wsz;
            const int i0 = (int) rp, i1 = (i0 + 1) % wsz;
            const float fr = rp - (float) i0;
            float x = age > 0.001f ? wowBuf[c][(size_t) i0] + fr * (wowBuf[c][(size_t) i1] - wowBuf[c][(size_t) i0]) : ch[c][i];
            ageLp[c] = x + aAge * (ageLp[c] - x); x = ageLp[c];
            if (rust > 0.001f) { if (take) hold[c] = std::round (x * steps) / steps; x = hold[c]; }
            ch[c][i] = x;
        }
        if (++wowW >= wsz) wowW = 0;
        if (earth > 0.0f)
        {
            rumble += 0.002f * ((rng.nextFloat() * 2.0f - 1.0f) - rumble);
            float e = rumble * 0.25f * earth;
            if (rng.nextFloat() < earth * 0.0006f) e += (rng.nextFloat() - 0.5f) * 0.25f * earth;   // Knistern
            e *= 0.3f + 0.7f * droneEnv;
            L[i] += e; R[i] += e * 0.9f;
        }
    }

    const float h = hall->load();
    tmk::FdnReverb::Settings rs;
    rs.sizeScale = 1.3f; rs.decay = 3.5f; rs.highRatio = 0.4f; rs.highCut = 3000.0f; rs.diffusion = 0.6f; rs.modDepth = 0.15f;
    reverb.update (rs);
    reverb.process (L, R, wet.getWritePointer (0), wet.getWritePointer (1), n);

    gainSm.setTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    const auto* wl = wet.getReadPointer (0); const auto* wr = wet.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSm.getNextValue();
        L[i] = std::tanh ((L[i] * (1.0f - 0.35f * h) + wl[i] * h) * g);
        R[i] = std::tanh ((R[i] * (1.0f - 0.35f * h) + wr[i] * h) * g);
    }
    scope.push (L, R, n);
}

juce::AudioProcessorEditor* KeltenProcessor::createEditor() { return new KeltenEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KeltenProcessor(); }

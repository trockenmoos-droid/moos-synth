#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<P>  (ID { "hydraulis", 1 },   "Hydraulis", 0.0f, 1.0f, 0.8f));
    p.push_back (std::make_unique<P>  (ID { "wasserdruck", 1 }, "Wasserdruck", 0.0f, 1.0f, 0.4f));
    p.push_back (std::make_unique<P>  (ID { "blubbern", 1 },    "Blubbern", 0.0f, 1.0f, 0.25f));
    p.push_back (std::make_unique<P>  (ID { "luft", 1 },        "Luft", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "reihen", 1 },      "Pfeifenreihen", 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "cornu", 1 },       "Cornu", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P>  (ID { "hell", 1 },        "Helligkeit", 0.0f, 1.0f, 0.45f));
    p.push_back (std::make_unique<P>  (ID { "anblasen", 1 },    "Anblasen", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "attack", 1 },      "Attack (s)", tmk::skewed (0.005f, 3.0f, 0.2f), 0.06f));
    p.push_back (std::make_unique<P>  (ID { "release", 1 },     "Release (s)", tmk::skewed (0.02f, 5.0f, 0.5f), 0.3f));
    p.push_back (std::make_unique<PC> (ID { "raum", 1 },        "Ort", juce::StringArray { "Villa", "Therme", "Arena" }, 1));
    p.push_back (std::make_unique<P>  (ID { "hall", 1 },        "Hall", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<P>  (ID { "volume", 1 },      u8 ("Lautst\xc3\xa4rke (dB)"), -48.0f, 6.0f, -6.0f));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Hydraulis in der Therme", {} },
        { "Arena-Fanfare",       { { "hydraulis", 0.0f }, { "cornu", 1.0f }, { "hell", 0.75f }, { "anblasen", 0.6f }, { "raum", 2 }, { "hall", 0.5f } } },
        { "Brodelnder Kessel",   { { "wasserdruck", 0.9f }, { "blubbern", 0.8f }, { "luft", 0.5f }, { "reihen", 1.0f } } },
        { "Villa am See",        { { "wasserdruck", 0.2f }, { "blubbern", 0.1f }, { "luft", 0.2f }, { "reihen", 0.0f }, { "raum", 0 }, { "hall", 0.25f },
                                   { "attack", 0.15f } } },
        { "Orgel und Horn",      { { "hydraulis", 0.6f }, { "cornu", 0.6f }, { "hell", 0.5f } } },
        { "Noricum bei Nacht",   { { "hydraulis", 0.7f }, { "reihen", 0.0f }, { "luft", 0.6f }, { "wasserdruck", 0.6f }, { "attack", 1.2f },
                                   { "release", 2.5f }, { "raum", 2 }, { "hall", 0.7f } } },
        { "Cornu weich",         { { "hydraulis", 0.0f }, { "cornu", 0.85f }, { "hell", 0.3f }, { "anblasen", 0.15f }, { "attack", 0.12f } } },
    };
}

RoemerProcessor::RoemerProcessor()
    : tmk::Processor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "ROEMER", factoryPresets())
{
    auto get = [this] (const char* i) { return apvts.getRawParameterValue (i); };
    hydraulis = get ("hydraulis"); wasserdruck = get ("wasserdruck"); blubbern = get ("blubbern"); luft = get ("luft"); reihen = get ("reihen");
    cornu = get ("cornu"); hell = get ("hell"); anblasen = get ("anblasen"); attack = get ("attack"); release = get ("release");
    raum = get ("raum"); hall = get ("hall"); volume = get ("volume");
}

bool RoemerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RoemerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    rp.sr = sampleRate;
    juce::uint32 seed = 117;
    for (auto& v : voices.voices) v.setup (sampleRate, &rp, seed++);
    voices.reset();
    keyboardState.reset();
    reverb.prepare (sampleRate);
    wet.setSize (2, samplesPerBlock);
    for (auto& b : bubbles) b.reset();
    press = pressTarget = 0.0f; act = 0.0f;
    gainSm.reset (sampleRate, 0.05);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
}

void RoemerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const double sr = rp.sr;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (wet.getNumSamples() < n) wet.setSize (2, n, false, false, true);

    rp.hydraulis = hydraulis->load(); rp.wasserdruck = wasserdruck->load(); rp.blubbern = blubbern->load(); rp.luft = luft->load();
    rp.reihen = reihen->load(); rp.cornu = cornu->load(); rp.hell = hell->load(); rp.anblasen = anblasen->load();
    const juce::ADSR::Parameters ep { attack->load(), 0.1f, 0.9f, release->load() };
    for (auto& v : voices.voices) v.env.setParameters (ep);

    // Wasserdruck: langsamer Zufallsweg (Kessel, Kolbenpumpe) – pro Block fortgeschrieben
    const float blockSec = (float) (n / sr);
    if (rng.nextFloat() < blockSec * 2.5f) pressTarget = rng.nextFloat() * 2.0f - 1.0f;
    press += (pressTarget - press) * juce::jmin (1.0f, blockSec * 3.0f);
    rp.pressure = press;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    voices.process (L, R, n, midi);

    // Blubbern im Wasserkessel: kurze, tiefe Resonanzen, nur während gespielt wird
    act += ((voices.activeCount() > 0 ? 1.0f : 0.0f) - act) * juce::jmin (1.0f, blockSec * 2.0f);
    const float bub = rp.blubbern * rp.hydraulis;
    if (bub > 0.0f && act > 0.01f)
        for (int i = 0; i < n; ++i)
        {
            if (rng.nextFloat() < bub * act * 0.0009f)
            {
                auto& b = bubbles[nextBubble]; nextBubble = (nextBubble + 1) % 4;
                b.set (180.0 + rng.nextFloat() * 520.0, 0.02 + rng.nextFloat() * 0.05, sr, 0.0f);
                b.gain = 0.012f * bub;
                b.y1 += 1.0f;
            }
            float s = 0.0f;
            for (auto& b : bubbles) s += b.process (0.0f);
            L[i] += s; R[i] += s * 0.8f;
        }

    const int place = juce::jlimit (0, 2, (int) raum->load());
    const float sizes[3] = { 0.6f, 1.6f, 2.8f }, decays[3] = { 0.9f, 3.8f, 2.2f }, highs[3] = { 0.6f, 0.75f, 0.35f };
    const float h = hall->load();
    tmk::FdnReverb::Settings rs;
    rs.sizeScale = sizes[place]; rs.decay = decays[place]; rs.highRatio = highs[place]; rs.highCut = 5000.0f;
    rs.diffusion = place == 2 ? 0.45f : 0.8f; rs.modDepth = 0.2f;
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

juce::AudioProcessorEditor* RoemerProcessor::createEditor() { return new RoemerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RoemerProcessor(); }

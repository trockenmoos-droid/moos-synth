#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

static juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

APVTS::ParameterLayout MoosProcessor::createLayout()
{
    using P  = juce::AudioParameterFloat;
    using PI = juce::AudioParameterInt;
    using ID = juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // Oszillator A
    p.push_back (std::make_unique<P>  (ID { "posA", 1 },   "Osc A Position", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<PI> (ID { "octA", 1 },   "Osc A Octave", -2, 2, 0));
    p.push_back (std::make_unique<P>  (ID { "levelA", 1 }, "Osc A Level", 0.0f, 1.0f, 0.8f));
    // Oszillator B
    p.push_back (std::make_unique<P>  (ID { "posB", 1 },   "Osc B Position", 0.0f, 1.0f, 0.6f));
    p.push_back (std::make_unique<PI> (ID { "octB", 1 },   "Osc B Octave", -2, 2, -1));
    p.push_back (std::make_unique<P>  (ID { "fineB", 1 },  "Osc B Fine (ct)", -100.0f, 100.0f, 7.0f));
    p.push_back (std::make_unique<P>  (ID { "levelB", 1 }, "Osc B Level", 0.0f, 1.0f, 0.5f));
    // Unison
    p.push_back (std::make_unique<PI> (ID { "uniVoices", 1 }, "Unison Voices", 1, PadVoice::maxUnison, 5));
    p.push_back (std::make_unique<P>  (ID { "uniDetune", 1 }, "Unison Detune (ct)", 0.0f, 60.0f, 14.0f));
    p.push_back (std::make_unique<P>  (ID { "uniWidth", 1 },  "Unison Width", 0.0f, 1.0f, 0.85f));
    // Filter
    p.push_back (std::make_unique<P> (ID { "cutoff", 1 }, "Cutoff (Hz)", skewed (20.0f, 20000.0f, 1200.0f), 1800.0f));
    p.push_back (std::make_unique<P> (ID { "reso", 1 },   "Resonance", 0.0f, 1.0f, 0.25f));
    p.push_back (std::make_unique<P> (ID { "envAmt", 1 }, "Filter Env Amount", -1.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P> (ID { "fA", 1 }, "Filter Attack (s)",  skewed (0.001f, 10.0f, 1.0f), 1.5f));
    p.push_back (std::make_unique<P> (ID { "fD", 1 }, "Filter Decay (s)",   skewed (0.001f, 10.0f, 1.0f), 2.5f));
    p.push_back (std::make_unique<P> (ID { "fS", 1 }, "Filter Sustain",     0.0f, 1.0f, 0.4f));
    p.push_back (std::make_unique<P> (ID { "fR", 1 }, "Filter Release (s)", skewed (0.001f, 15.0f, 1.5f), 3.0f));
    // Amp-Hüllkurve
    p.push_back (std::make_unique<P> (ID { "aA", 1 }, "Amp Attack (s)",  skewed (0.001f, 10.0f, 1.0f), 0.8f));
    p.push_back (std::make_unique<P> (ID { "aD", 1 }, "Amp Decay (s)",   skewed (0.001f, 10.0f, 1.0f), 1.0f));
    p.push_back (std::make_unique<P> (ID { "aS", 1 }, "Amp Sustain",     0.0f, 1.0f, 0.85f));
    p.push_back (std::make_unique<P> (ID { "aR", 1 }, "Amp Release (s)", skewed (0.001f, 15.0f, 1.5f), 3.5f));
    // LFO
    p.push_back (std::make_unique<P> (ID { "lfoRate", 1 }, "LFO Rate (Hz)", skewed (0.01f, 10.0f, 0.5f), 0.12f));
    p.push_back (std::make_unique<P> (ID { "lfoPos", 1 },  "LFO -> Position", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<P> (ID { "lfoCut", 1 },  "LFO -> Cutoff", 0.0f, 1.0f, 0.15f));
    // Effekte
    p.push_back (std::make_unique<P> (ID { "chorusMix", 1 },  "Chorus Mix", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<P> (ID { "reverbSize", 1 }, "Reverb Size", 0.0f, 1.0f, 0.85f));
    p.push_back (std::make_unique<P> (ID { "reverbMix", 1 },  "Reverb Mix", 0.0f, 1.0f, 0.4f));
    p.push_back (std::make_unique<P> (ID { "volume", 1 },     "Volume (dB)", -48.0f, 6.0f, -6.0f));

    return { p.begin(), p.end() };
}

MoosProcessor::MoosProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MOOS", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    params.posA = get ("posA");   params.octA = get ("octA");   params.levelA = get ("levelA");
    params.posB = get ("posB");   params.octB = get ("octB");   params.fineB = get ("fineB"); params.levelB = get ("levelB");
    params.uniVoices = get ("uniVoices"); params.uniDetune = get ("uniDetune"); params.uniWidth = get ("uniWidth");
    params.cutoff = get ("cutoff"); params.reso = get ("reso"); params.envAmt = get ("envAmt");
    params.fA = get ("fA"); params.fD = get ("fD"); params.fS = get ("fS"); params.fR = get ("fR");
    params.aA = get ("aA"); params.aD = get ("aD"); params.aS = get ("aS"); params.aR = get ("aR");
    params.lfoRate = get ("lfoRate"); params.lfoPos = get ("lfoPos"); params.lfoCut = get ("lfoCut");

    chorusMix  = get ("chorusMix");
    reverbSize = get ("reverbSize");
    reverbMix  = get ("reverbMix");
    volume     = get ("volume");

    for (int i = 0; i < 16; ++i)
        synth.addVoice (new PadVoice (bank, params));
    synth.addSound (new PadSound());
}

bool MoosProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MoosProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    keyboardState.reset();

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    chorus.prepare (spec);
    chorus.setRate (0.25f);
    chorus.setDepth (0.35f);
    chorus.setCentreDelay (9.0f);
    chorus.setFeedback (0.1f);
    reverb.prepare (spec);

    masterGain.reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load()));
}

void MoosProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    synth.renderNextBlock (buffer, midi, 0, n);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    chorus.setMix (chorusMix->load());
    chorus.process (ctx);

    juce::dsp::Reverb::Parameters rp;
    rp.roomSize   = reverbSize->load();
    rp.damping    = 0.45f;
    rp.wetLevel   = reverbMix->load();
    rp.dryLevel   = 1.0f - 0.5f * reverbMix->load();
    rp.width      = 1.0f;
    reverb.setParameters (rp);
    reverb.process (ctx);

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[i] = std::tanh (d[i] * g);       // sanfter Soft-Clip als Schutz vor Übersteuerung
        }
    }
}

juce::AudioProcessorEditor* MoosProcessor::createEditor()
{
    return new MoosEditor (*this);
}

void MoosProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MoosProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoosProcessor();
}

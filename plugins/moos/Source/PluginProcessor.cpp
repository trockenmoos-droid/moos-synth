#include "PluginProcessor.h"
#include "PluginEditor.h"

MoosProcessor::MoosProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MOOS", MoosParams::createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    hpfHz = get ("hpf"); chorusMix = get ("chorusMix"); reverbSize = get ("reverbSize");
    reverbMix = get ("reverbMix"); shimmer = get ("shimmer"); volume = get ("volume");
    delaySync = get ("delaySync"); delayDiv = get ("delayDiv"); delayTime = get ("delayTime");
    delayFb = get ("delayFb"); delayMix = get ("delayMix");
    arpOn = get ("arpOn"); arpMode = get ("arpMode"); arpRate = get ("arpRate"); arpOct = get ("arpOct"); arpGate = get ("arpGate");

    factoryBank = WavetableBank::createFactory();
    engine.init (apvts);
    engine.setBank (0, factoryBank.get());
    engine.setBank (1, factoryBank.get());
    apvts.state.setProperty ("presetName", "Init Moos", nullptr);
}

bool MoosProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MoosProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    arp.prepare (sampleRate);
    keyboardState.reset();

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (samplesPerBlock, maxChunk), 2 };
    chorus.prepare (spec);
    chorus.setRate (0.25f);
    chorus.setDepth (0.35f);
    chorus.setCentreDelay (9.0f);
    chorus.setFeedback (0.1f);

    hpf.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate, samplesPerBlock);

    masterGain.reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
}

// Die DAW kann größere Blöcke schicken als angekündigt (z. B. beim Preset-Wechsel).
// Der JUCE-Chorus verträgt das nicht – deshalb wird in Häppchen bis maxChunk verarbeitet.
void MoosProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (needsReset.exchange (false))
        prepareToPlay (getSampleRate() > 0.0 ? getSampleRate() : 48000.0, maxChunk);

    const int total = buffer.getNumSamples();
    if (total <= maxChunk) processChunk (buffer, midi, 0);
    else
        for (int off = 0; off < total; off += maxChunk)
        {
            const int n = juce::jmin (maxChunk, total - off);
            float* ptrs[2] = { buffer.getWritePointer (0, off), buffer.getWritePointer (juce::jmin (1, buffer.getNumChannels() - 1), off) };
            juce::AudioBuffer<float> part (ptrs, juce::jmin (2, buffer.getNumChannels()), n);
            juce::MidiBuffer partMidi;
            for (const auto m : midi)
                if (m.samplePosition >= off && m.samplePosition < off + n)
                    partMidi.addEvent (m.getMessage(), m.samplePosition - off);
            processChunk (part, partMidi, off);
        }

    // Sicherheitsnetz: nie NaN/Inf ausgeben, sondern still sein und den Zustand neu aufsetzen
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < total; ++i)
            if (! std::isfinite (buffer.getSample (c, i))) { buffer.clear(); needsReset = true; return; }
}

void MoosProcessor::processChunk (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, int chunkOffset)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    // Tempo / Transport vom Host
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p + chunkOffset * bpm / 60.0 / getSampleRate();
            playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }

    Arpeggiator::Settings as { arpOn->load() > 0.5f, (int) arpMode->load(), (int) arpRate->load(),
                               (int) std::round (arpOct->load()), arpGate->load(), bpm, playing, ppq };
    arp.process (midi, arpMidi, n, as);

    engine.process (buffer, arpMidi, n);

    hpf.process (buffer, 0, n, hpfHz->load());

    juce::dsp::AudioBlock<float> block (buffer);
    chorus.setMix (chorusMix->load());
    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));

    const float dTime = delaySync->load() > 0.5f
                          ? (float) (MoosParams::delayDivBeats ((int) delayDiv->load()) * 60000.0 / bpm)
                          : delayTime->load();
    delay.process (buffer, 0, n, juce::jmin (dTime, 2000.0f), delayFb->load(), delayMix->load());

    reverb.process (buffer, 0, n, reverbSize->load(), reverbMix->load(), shimmer->load());

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[i] = std::tanh (d[i] * g);       // sanfter Soft-Clip als Schutz
        }
    }
}

//==============================================================================
juce::String MoosProcessor::loadWavetable (int osc, const juce::File& file)
{
    juce::String error;
    auto bank = WavetableBank::createFromFile (file, error);
    if (bank == nullptr) return error.isEmpty() ? "Unbekannter Fehler" : error;

    const juce::ScopedLock sl (bankLock);
    engine.setBank (osc, bank.get());
    loadedBanks.push_back (std::move (bank));
    apvts.state.setProperty (osc == 0 ? "waveA" : "waveB", file.getFullPathName(), nullptr);
    return {};
}

void MoosProcessor::resetWavetable (int osc)
{
    engine.setBank (osc, factoryBank.get());
    apvts.state.removeProperty (osc == 0 ? "waveA" : "waveB", nullptr);
}

void MoosProcessor::restoreWavetables()
{
    for (int osc = 0; osc < 2; ++osc)
    {
        const auto path = apvts.state.getProperty (osc == 0 ? "waveA" : "waveB").toString();
        if (path.isNotEmpty() && juce::File (path).existsAsFile())
        {
            if (loadWavetable (osc, juce::File (path)).isNotEmpty())
                engine.setBank (osc, factoryBank.get());
        }
        else
            engine.setBank (osc, factoryBank.get());
    }
}

juce::AudioProcessorEditor* MoosProcessor::createEditor()
{
    return new MoosEditor (*this);
}

//==============================================================================
int MoosProcessor::getNumPrograms() { return (int) MoosPresets::factory().size(); }

const juce::String MoosProcessor::getProgramName (int i)
{
    const auto& f = MoosPresets::factory();
    return juce::isPositiveAndBelow (i, (int) f.size()) ? juce::String (f[(size_t) i].name) : juce::String();
}

void MoosProcessor::setCurrentProgram (int i) { loadFactoryPreset (i); }

void MoosProcessor::loadFactoryPreset (int index)
{
    const auto& f = MoosPresets::factory();
    if (! juce::isPositiveAndBelow (index, (int) f.size())) return;
    currentProgram = index;
    MoosPresets::apply (apvts, f[(size_t) index]);
    resetWavetable (0);
    resetWavetable (1);
    apvts.state.setProperty ("presetName", f[(size_t) index].name, nullptr);
    ++presetChangeCounter;
}

juce::String MoosProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return "Keine gültige MOOS-Preset-Datei";

    // zuerst alles auf Standard, damit fehlende (ältere) Parameter definiert sind
    MoosPresets::apply (apvts, MoosPresets::factory()[0]);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    restoreWavetables();
    ++presetChangeCounter;
    return {};
}

bool MoosProcessor::saveUserPreset (const juce::File& file)
{
    apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    if (auto xml = apvts.copyState().createXml())
    {
        ++presetChangeCounter;
        return xml->writeTo (file);
    }
    return false;
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
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            restoreWavetables();
            ++presetChangeCounter;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoosProcessor();
}

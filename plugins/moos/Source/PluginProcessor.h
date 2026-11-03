#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Wavetable.h"
#include "SynthEngine.h"
#include "Arpeggiator.h"
#include "Effects.h"
#include "Presets.h"
#include "MossBackground.h"

class MoosProcessor : public juce::AudioProcessor
{
public:
    MoosProcessor();
    ~MoosProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MOOS"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- für die Oberfläche ---
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    const WavetableBank* getBank (int osc) const { return engine.getBank (osc); }
    juce::String loadWavetable (int osc, const juce::File& file);    // "" = ok, sonst Fehlermeldung
    void resetWavetable (int osc);

    void loadFactoryPreset (int index);
    juce::String loadUserPreset (const juce::File& file);
    bool saveUserPreset (const juce::File& file);
    juce::String getPresetName() const { return apvts.state.getProperty ("presetName", "Init Moos").toString(); }

    float getLfoValue (int i) const { return engine.getLfoValue (i); }
    std::atomic<int> presetChangeCounter { 0 };

private:
    std::unique_ptr<WavetableBank> factoryBank;
    std::vector<std::unique_ptr<WavetableBank>> loadedBanks;   // bleiben am Leben (Audio-Thread liest evtl. noch)
    juce::CriticalSection bankLock;

    SynthEngine engine;
    Arpeggiator arp;
    juce::MidiBuffer arpMidi;

    HighPass hpf;
    juce::dsp::Chorus<float> chorus;
    PingPongDelay delay;
    ShimmerReverb reverb;
    juce::SmoothedValue<float> masterGain;

    int currentProgram = 0;
    juce::SharedResourcePointer<MossBackground::MossCache> mossCache;   // Textur schon vorab rechnen

    using AF = std::atomic<float>*;
    AF hpfHz{}, chorusMix{}, reverbSize{}, reverbMix{}, shimmer{}, volume{};
    AF delaySync{}, delayDiv{}, delayTime{}, delayFb{}, delayMix{};
    AF arpOn{}, arpMode{}, arpRate{}, arpOct{}, arpGate{};

    void restoreWavetables();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoosProcessor)
};

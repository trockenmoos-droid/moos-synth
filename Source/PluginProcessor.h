#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Wavetable.h"
#include "SynthVoice.h"

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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    const WavetableBank& getBank() const { return bank; }

private:
    WavetableBank bank;
    PadParams params;
    juce::Synthesiser synth;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> masterGain;

    std::atomic<float>* chorusMix {};
    std::atomic<float>* reverbSize {};
    std::atomic<float>* reverbMix {};
    std::atomic<float>* volume {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoosProcessor)
};

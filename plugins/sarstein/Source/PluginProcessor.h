#pragma once

#include <tm/TmProcessor.h>
#include <juce_dsp/juce_dsp.h>

// SARSTEIN – Verzerrer: vier Gesteinsarten als Kennlinien, Geröll (Asymmetrie),
// Steinschlag (Bit- und Ratenreduktion), 4-faches Oversampling gegen Aliasing.
namespace sarstein
{
    inline const juce::StringArray stones { "Kalk", "Granit", "Fels", "Bergkristall" };

    // Kennlinie je Gestein (x bereits verstärkt)
    inline float shape (int stone, float x) noexcept
    {
        switch (stone)
        {
            case 1:  return x / std::pow (1.0f + std::pow (std::abs (x), 6.0f), 1.0f / 6.0f);              // Granit: hart
            case 2:  return x > 0.0f ? 1.0f - std::exp (-x) : -(1.0f - std::exp (x * 1.6f)) / 1.6f;        // Fels: röhrig, asymmetrisch
            case 3:  return 0.95f * std::sin (1.4f * x) * (1.0f / (1.0f + 0.02f * x * x)) + 0.05f * std::tanh (x); // Bergkristall: Faltung
            default: return std::tanh (x);                                                                  // Kalk: weich
        }
    }
}

class SarsteinProcessor : public tmk::Processor
{
public:
    SarsteinProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };

private:
    double sr = 48000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> dry;
    std::vector<float> driveCurve, dryRing[2];
    int dryW = 0;
    float hp1[2] {}, hp2[2] {}, dc[2] {}, dcIn[2] {}, lp[2] {}, tiltLp[2] {}, hold[2] {};
    int holdCount = 0;
    juce::SmoothedValue<float> driveSm, outSm, mixSm;

    using AF = std::atomic<float>*;
    AF stein{}, drive{}, geroell{}, steinschlag{}, tiefe{}, ton{}, highcut{}, mix{}, output{}, autogain{};
};

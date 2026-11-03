#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmReverb.h>
#include <tm/TmVoices.h>

// GOISERER PASCHER – Rhythmus-Gate und Klatscher-Gruppe ("Paschen" = rhythmisches Klatschen
// in der Salzkammergut-Volksmusik). Zwei Reihen à 16 Schritte, im Takt der DAW.
namespace pascher
{
    constexpr int numSteps = 16;
    inline const juce::StringArray rates { "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    inline double rateBeats (int i) { const double v[] = { 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 }; return v[juce::jlimit (0, 4, i)]; }
    inline juce::String gateId (int s)  { return "g" + juce::String (s + 1); }
    inline juce::String clapId (int s)  { return "c" + juce::String (s + 1); }
}

class PascherProcessor : public tmk::Processor
{
public:
    PascherProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    std::atomic<int> currentStep { -1 };

private:
    struct Clap
    {
        bool active = false;
        int delay = 0, age = 0, bursts = 3;
        float gain = 0.0f, panL = 0.7f, panR = 0.7f;
        tmk::dsp::Svf filter;
    };

    void triggerClaps (int sampleOffset, float accent);

    double sr = 48000.0;
    double freePpq = 0.0;
    int lastStep = -1;
    float gateGain = 1.0f;
    Clap claps[24];
    juce::Random rng;
    tmk::FdnReverb reverb;
    juce::AudioBuffer<float> clapBuf, wet;
    juce::SmoothedValue<float> mixSm;

    using AF = std::atomic<float>*;
    AF rate{}, laenge{}, swing{}, schmuggel{}, tiefe{}, glaetten{}, mix{}, klatsch{}, klatscher{}, mensch{}, ton{}, raum{}, output{};
    AF gates[pascher::numSteps]{}, clapSteps[pascher::numSteps]{};
};

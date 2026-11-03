#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmReverb.h>

// HALLSTATT – Hall mit fünf Räumen aus dem Salzkammergut
namespace hallstatt
{
    inline const juce::StringArray roomNames {
        juce::String::fromUTF8 ("Salzbergwerk"), juce::String::fromUTF8 ("Beinhaus"),
        juce::String::fromUTF8 ("Pfarrkirche"),  juce::String::fromUTF8 ("Hallst\xc3\xa4ttersee"),
        juce::String::fromUTF8 ("Eish\xc3\xb6hle") };

    struct Tap { float ms, gain, pan; };

    // Charakter je Raum: Grundgröße, Diffusion, frühe Reflexionen, Klangfarbe
    struct Room
    {
        float sizeScale, diffusion, erLowpass, modRate;
        float colourFreq, colourGainDb, colourQ;
        std::vector<Tap> taps;
    };

    inline const std::vector<Room>& rooms()
    {
        static const std::vector<Room> r = {
            // Salzbergwerk: langer Stollen, Flatterecho, dumpfer Bauch
            { 1.35f, 0.55f, 6000.0f, 0.35f, 320.0f, 4.0f, 0.9f,
              { { 11, 0.55f, -0.3f }, { 23, 0.5f, 0.3f }, { 46, 0.42f, -0.25f }, { 69, 0.36f, 0.25f }, { 92, 0.3f, -0.2f },
                { 115, 0.25f, 0.2f }, { 138, 0.2f, -0.15f }, { 161, 0.16f, 0.15f }, { 184, 0.12f, -0.1f } } },
            // Beinhaus: kleine Kapelle, dicht, hell
            { 0.42f, 0.8f, 11000.0f, 0.7f, 2500.0f, 2.0f, 0.7f,
              { { 3.1f, 0.6f, -0.6f }, { 4.7f, 0.5f, 0.7f }, { 7.9f, 0.45f, -0.2f }, { 11.3f, 0.4f, 0.5f }, { 13.8f, 0.35f, -0.7f },
                { 17.2f, 0.3f, 0.2f }, { 21.5f, 0.25f, -0.4f }, { 26.0f, 0.2f, 0.6f } } },
            // Pfarrkirche: großer Raum, warm
            { 2.1f, 0.85f, 7000.0f, 0.5f, 180.0f, 2.0f, 0.7f,
              { { 17, 0.5f, -0.5f }, { 26, 0.45f, 0.6f }, { 38, 0.4f, -0.2f }, { 51, 0.35f, 0.4f }, { 63, 0.3f, -0.6f },
                { 77, 0.26f, 0.1f }, { 92, 0.22f, 0.5f } } },
            // Hallstättersee: offen, Echos von den Felswänden über das Wasser
            { 1.8f, 0.4f, 2400.0f, 0.25f, 900.0f, -3.0f, 0.5f,
              { { 180, 0.45f, -0.7f }, { 265, 0.32f, 0.8f }, { 340, 0.3f, -0.3f }, { 470, 0.22f, 0.6f }, { 610, 0.16f, -0.8f }, { 760, 0.1f, 0.4f } } },
            // Eishöhle: riesig, hell, gläsern
            { 2.6f, 0.6f, 12000.0f, 0.18f, 4200.0f, 3.0f, 0.8f,
              { { 9, 0.4f, -0.4f }, { 19, 0.38f, 0.5f }, { 31, 0.35f, -0.6f }, { 44, 0.3f, 0.3f }, { 58, 0.26f, -0.2f }, { 73, 0.22f, 0.7f } } },
        };
        return r;
    }
}

class HallstattProcessor : public tmk::Processor
{
public:
    HallstattProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    std::atomic<float> inputLevel { 0.0f }, wetLevel { 0.0f };

private:
    tmk::FdnReverb fdn;
    double sr = 48000.0;

    std::vector<float> pre[2];
    int preW = 0;
    float erLp[2] {}, hpS[2][2] {}, lpS[2] {}, duckEnv = 0.0f;
    juce::dsp::IIR::Filter<float> colour[2];
    int lastRoom = -1;
    float lastColour = -999.0f;
    juce::AudioBuffer<float> wet, fdnIn, fdnOut;
    juce::SmoothedValue<float> mixSm;

    using AF = std::atomic<float>*;
    AF room{}, size{}, decay{}, predelay{}, damping{}, bass{}, mod{}, early{}, width{}, lowcut{}, highcut{}, mix{}, freeze{}, duck{};
};

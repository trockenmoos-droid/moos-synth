#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmReverb.h>

// DACHSTEIN – Gletscher-Drone-Synth: additive Obertöne (bis 32, dehnbar zu glockigen,
// "kristallinen" Spektren), langsame Bewegung jedes Teiltons, Fels (Sub), Wind, Eisknistern, Hall.
namespace dachstein
{
    constexpr int maxVoices = 8;
    constexpr int maxPartials = 32;
    constexpr int sineSize = 4096;
}

class DachsteinProcessor : public tmk::Processor
{
public:
    DachsteinProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    juce::MidiKeyboardState keyboardState;

    // für die Anzeige: Frequenzverhältnis und aktuelle Amplitude jedes Teiltons
    std::atomic<float> partialRatio[dachstein::maxPartials] {}, partialAmp[dachstein::maxPartials] {};
    std::atomic<float> activity { 0.0f };

private:
    struct Voice
    {
        bool active = false, held = false, sustained = false;
        int note = 60;
        float velocity = 1.0f;
        juce::uint64 age = 0;
        double phase[dachstein::maxPartials][2] {};
        float drift[dachstein::maxPartials] {}, driftTarget[dachstein::maxPartials] {};
        double subPhase = 0.0;
        juce::ADSR env;
    };

    void handleMidi (const juce::MidiMessage&);
    void renderVoices (float* L, float* R, int start, int n);
    void renderAmbience (float* L, float* R, int start, int n);

    double sr = 48000.0;
    Voice voices[dachstein::maxVoices];
    juce::uint64 ageCounter = 0;
    bool sustainPedal = false;
    float bend = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    std::vector<float> sine;
    float pan[dachstein::maxPartials][2] {};
    float detuneSeed[dachstein::maxPartials] {};
    juce::Random rng;

    // Block-Werte
    float ratio[dachstein::maxPartials] {}, amp[dachstein::maxPartials] {};
    int numPartials = 24;
    float ampNorm = 1.0f, movement = 0.5f, moveRate = 0.15f, melt = 0.3f;

    // Wind und Knistern (global)
    float windLp = 0.0f, windIc1[2] {}, windIc2[2] {}, gust = 0.0f, gustTarget = 0.0f, act = 0.0f;
    struct Ping { float y1 = 0, y2 = 0, b1 = 0, b2 = 0, gainL = 0, gainR = 0; };
    Ping pings[6];
    int nextPing = 0;

    tmk::FdnReverb reverb;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> gainSm;

    using AF = std::atomic<float>*;
    AF obertoene{}, kristall{}, spalten{}, teiltoene{}, bewegung{}, tempo{}, schmelze{}, breite{}, fels{},
       wind{}, windFarbe{}, boeen{}, knistern{}, attack{}, release{}, hall{}, volume{};
};

#pragma once

#include <tm/TmProcessor.h>

// 5 FINGERS – Delay mit fünf Abgriffen ("Fingern") wie die Aussichtsplattform am Krippenstein.
// Jeder Finger hat Zeit, Pegel, Panorama und Körnung: Körnung zerlegt sein Echo in eine
// Granular-Wolke (Korngröße, Tonhöhe, Streuung, Dichte, Rückwärts).
namespace fingers
{
    constexpr int numFingers = 5;
    constexpr int maxGrains = 10;
    inline const juce::StringArray divisions { "1/8", "1/4", "1/4D", "1/2", "1/2D", "1/1", "2/1" };
    inline double divisionBeats (int i)
    {
        const double v[] = { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0 };
        return v[juce::jlimit (0, 6, i)];
    }
    inline juce::String id (const char* base, int finger) { return juce::String (base) + juce::String (finger + 1); }
}

class FingersProcessor : public tmk::Processor
{
public:
    FingersProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    std::atomic<float> fingerLevel[fingers::numFingers] {};
    std::atomic<int>   grainCount[fingers::numFingers] {};
    std::atomic<float> baseMs { 1000.0f };

private:
    struct Grain
    {
        bool active = false;
        double delay = 0.0;      // aktuelle Leseposition (Samples hinter dem Schreibkopf)
        double step = 0.0;       // Änderung der Verzögerung pro Sample (Tonhöhe / rückwärts)
        int age = 0, length = 1;
        float gainL = 0.0f, gainR = 0.0f;
    };
    struct Finger
    {
        Grain grains[fingers::maxGrains];
        double untilNext = 0.0;
    };

    float readMono (double delay) const noexcept;
    float readCh (int ch, double delay) const noexcept;

    double sr = 48000.0;
    std::vector<float> buf[2];
    int w = 0;
    Finger fingerState[fingers::numFingers];
    juce::SmoothedValue<float> baseSm, mixSm;
    float fbLp[2] {}, outLp[2] {};
    juce::Random rng;

    using AF = std::atomic<float>*;
    AF sync{}, division{}, timeMs{}, feedback{}, ton{}, korngroesse{}, tonhoehe{}, streuung{}, dichte{}, rueckwaerts{}, breite{}, mix{};
    AF zeit[fingers::numFingers]{}, pegel[fingers::numFingers]{}, pan[fingers::numFingers]{}, korn[fingers::numFingers]{};
};

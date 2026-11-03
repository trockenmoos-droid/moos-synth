#pragma once

#include <tm/TmProcessor.h>

// SOLELEITUNG – Delay mit Charakter: jede Wiederholung fließt durch Rohr (Resonanz),
// Salz (Sättigung) und Strömung (Wow/Flutter). Bis zu vier "Stationen" als Abgriffe.
namespace sole
{
    inline const juce::StringArray divisions { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
    inline double divisionBeats (int i)
    {
        const double v[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
        return v[juce::jlimit (0, 8, i)];
    }
    // Orte entlang der Leitung: Hallstatt -> Bad Goisern -> Lauffen -> Bad Ischl -> Ebensee
    inline juce::String stationName (int station, int count)
    {
        static const char* names[4][4] = { { "Ebensee" }, { "Bad Ischl", "Ebensee" },
                                           { "Bad Goisern", "Bad Ischl", "Ebensee" },
                                           { "Bad Goisern", "Lauffen", "Bad Ischl", "Ebensee" } };
        return juce::String::fromUTF8 (names[juce::jlimit (1, 4, count) - 1][juce::jlimit (0, count - 1, station)]);
    }
}

class SoleleitungProcessor : public tmk::Processor
{
public:
    SoleleitungProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    // für die Anzeige
    std::atomic<float> stationLevel[4] {};
    std::atomic<float> delayMs { 500.0f };

private:
    double sr = 48000.0;
    std::vector<float> buf[2];
    int w = 0;
    juce::SmoothedValue<float> timeSm;      // Verzögerung in Samples (gleitet wie ein Band)
    juce::SmoothedValue<float> mixSm;

    struct ChannelState { float ic1 = 0, ic2 = 0, hp = 0, lp = 0; };
    ChannelState cs[2];
    float duckEnv = 0.0f;
    double wowPhase = 0.0, flutterPhase = 0.0;
    float drift = 0.0f, driftTarget = 0.0f;
    juce::Random rng;

    using AF = std::atomic<float>*;
    AF sync{}, division{}, timeMs{}, feedback{}, stations{}, rohr{}, rohrTon{}, salz{}, stroemung{},
       ton{}, lowcut{}, breite{}, duck{}, mix{}, absperren{};

    float read (int ch, float delay) const noexcept;
};

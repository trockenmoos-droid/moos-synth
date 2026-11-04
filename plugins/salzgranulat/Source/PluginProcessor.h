#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmVoices.h>

// SALZGRANULAT – Granulator, der zwischen Sole und Kristall wandert.
// Sole: Körner treiben zufällig in Zeit, Tonhöhe und Raum (eine Wolke).
// Kristall: dieselben Körner rasten in ein Gitter ein – Zeitpunkte auf das Taktraster der DAW,
// Tonhöhen auf einen Akkord, Panorama auf feste Gitterplätze, Lesepositionen auf ganze Schläge.
// Jedes Korn kann als kleiner Kristall nachklingen (gestimmter Resonator), beim „Verdunsten“
// nach oben gleiten und über „Rekristallisation“ wieder ins Salzlager zurückfallen.
namespace salz
{
    constexpr int maxGrains = 64;
    constexpr int combLen = 2048;          // Resonator je Korn (reicht bis ~24 Hz)
    inline const juce::StringArray lattices { "Oktaven", "Quinten", "Dur", "Moll", "Pentatonik", "Ganzton", "Cluster" };
    inline const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
    inline const std::vector<int>& latticeSteps (int i)
    {
        static const std::vector<int> l[] = {
            { 0 }, { 0, 7 }, { 0, 4, 7 }, { 0, 3, 7 }, { 0, 2, 4, 7, 9 }, { 0, 2, 4, 6, 8, 10 }, { 0, 1, 2, 3, 5, 6 }
        };
        return l[juce::jlimit (0, 6, i)];
    }
    // nächster Gitterton (Halbtöne relativ zum Grundton)
    inline float snap (float semis, int lattice)
    {
        const auto& st = latticeSteps (lattice);
        float best = semis, bestD = 1.0e9f;
        const int oct = (int) std::floor (semis / 12.0f);
        for (int o = oct - 1; o <= oct + 1; ++o)
            for (int s : st)
            {
                const float c = (float) (o * 12 + s), d = std::abs (c - semis);
                if (d < bestD) { bestD = d; best = c; }
            }
        return best;
    }
}

class SalzProcessor : public tmk::Processor
{
public:
    SalzProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    // für die Gitter-Anzeige: je Korn Lage im Raum (x = Panorama, y = Tonhöhe, z = Tiefe im Salzlager) und Leuchten
    struct View { std::atomic<float> x { 0 }, y { 0 }, z { 0 }, glow { 0 }; };
    View view[salz::maxGrains];
    std::atomic<float> crystalNow { 0.0f }, inputLevel { 0.0f };
    std::atomic<int> activeGrains { 0 };

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0;          // Leseposition (Index im Lager)
        double step = 1.0;         // Tonhöhe (negativ = rückwärts)
        double glide = 1.0;        // Verdunstung: Faktor pro Sample
        int age = 0, len = 1;
        float gl = 0.0f, gr = 0.0f, amp = 1.0f, shape = 0.5f;
        // Kristall-Resonator
        int combD = 100; float combFrac = 0.0f; float combFb = 0.0f, combMix = 0.0f; int cw = 0; float lp = 0.0f;
        float vx = 0, vy = 0, vz = 0;
    };

    void spawn (bool burst);
    float read (int ch, double pos) const noexcept;

    double sr = 48000.0;
    juce::Random rng;
    std::vector<float> lager[2];               // Salzlager: die letzten 12 s
    int w = 0, lagerSize = 1;
    Grain grains[salz::maxGrains];
    std::vector<float> combs;                  // maxGrains * combLen
    double untilGrain = 0.0;
    long long sampleClock = 0;
    double lastGridSlot = -1.0;
    // Anschlag (Transienten)
    float envFast = 0.0f, envSlow = 0.0f; int holdOff = 0, silentSamples = 0;
    // Rückführung
    float fbHp[2] {}, fbHpX[2] {}, lastWet[2] {};
    juce::SmoothedValue<float> mixSm, outSm;
    float wetGainNorm = 1.0f;

    using AF = std::atomic<float>*;
    AF korn{}, dichte{}, tiefe{}, streuung{}, rueckwaerts{};
    AF kristall{}, gitter{}, grundton{}, lage{}, klang{};
    AF verdunstung{}, rekristall{}, anschlag{}, kruste{};
    AF breite{}, mix{}, ausgang{};
};

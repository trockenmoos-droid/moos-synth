#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmVoices.h>

// HAMMERSCHMIED – generative Drum-Maschine (benannt nach der alten Hammerschmiede in Goisern).
// Sechs Spuren mit Schmiede-Klängen, jede mit eigenem euklidischem Muster und eigenem Tempo
// (Polymetrik), dazu Chaos (Timing, Wirbel), Mutation (Muster wandern) und "Glut":
// Granular-Wolke und Stotter-Wiederholungen über dem ganzen Schlagwerk.
namespace hammer
{
    constexpr int numTracks = 6;
    inline const char* trackNames[numTracks] = { "HAMMER", "AMBOSS", "BLASEBALG", "ESSE", "FUNKE", "ZANGE" };
    inline const char* trackKeys[numTracks]  = { "t1", "t2", "t3", "t4", "t5", "t6" };
    inline const juce::StringArray rates { "1/2", "2/3", "3/4", "1", "4/3", "3/2", "2" };
    inline double rateFactor (int i) { const double v[] = { 0.5, 2.0 / 3.0, 0.75, 1.0, 4.0 / 3.0, 1.5, 2.0 }; return v[juce::jlimit (0, 6, i)]; }
    inline juce::String pid (int track, const char* p) { return juce::String (trackKeys[track]) + p; }
    // euklidischer Rhythmus (Bresenham-Variante): ist Schritt i ein Schlag?
    inline bool euclid (int i, int steps, int hits, int rot)
    {
        if (hits <= 0 || steps <= 0) return false;
        if (hits >= steps) return true;
        const int k = ((i + rot) % steps + steps) % steps;
        return (k * hits) % steps < hits;
    }
}

class HammerProcessor : public tmk::Processor
{
public:
    HammerProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    // für die Ring-Anzeige
    std::atomic<int>   curStep[hammer::numTracks] {};
    std::atomic<float> hitFlash[hammer::numTracks] {};
    std::atomic<int>   rotOffset[hammer::numTracks] {}, hitOffset[hammer::numTracks] {};
    std::atomic<bool>  stuttering { false };

    struct Hit
    {
        bool active = false;
        int type = 0;
        int age = 0;
        float vel = 1.0f, pan = 0.0f, decay = 0.3f;
        double baseHz = 100.0;
        double ph[6] {}, freq[6] {};
        float amp[6] {}, coef[6] {};
        float env = 1.0f, envCoef = 0.999f, noiseEnv = 0.0f, noiseCoef = 0.99f;
        float fmRatio = 1.4f, fmIndex = 2.0f, fmCoef = 0.999f;
        tmk::dsp::Svf f1, f2;
    };

private:
    void trigger (int track, int offset, float vel);
    float renderHit (Hit& h) noexcept;

    double sr = 48000.0;
    juce::Random rng;
    Hit hits[hammer::numTracks][3];
    int nextSlot[hammer::numTracks] {};

    // Sequenzer
    double freePpq = 0.0;
    double lastTrackPos[hammer::numTracks] {};
    bool havePos = false;
    struct Pending { int samples; float vel; int track; };
    std::vector<Pending> pending;
    int lastBar = -1, lastBaseStep = -1;
    int rotOff[hammer::numTracks] {}, hitOff[hammer::numTracks] {};

    // Glut: Granular + Stotter
    std::vector<float> rec[2];
    int recW = 0;
    struct Grain { bool active = false; double pos = 0.0, step = 1.0; int age = 0, len = 1; float gl = 0, gr = 0; };
    Grain grains[16];
    double untilGrain = 0.0;
    struct Stutter { bool active = false; int sliceStart = 0, sliceLen = 1, pos = 0, repeats = 0; double rpos = 0.0, rate = 1.0; float gain = 1.0f; } stut;
    float stutMix = 0.0f;

    juce::SmoothedValue<float> gainSm;
    juce::AudioBuffer<float> dryBuf;

    using AF = std::atomic<float>*;
    AF frei{}, swing{}, chaos{}, mutation{}, hitze{}, volume{};
    AF glut{}, korn{}, dichte{}, tonhoehe{}, rueckwaerts{}, stotter{}, streuung{};
    AF level[hammer::numTracks]{}, tune[hammer::numTracks]{}, decay[hammer::numTracks]{}, steps[hammer::numTracks]{},
       hitsP[hammer::numTracks]{}, rotate[hammer::numTracks]{}, chance[hammer::numTracks]{}, rate[hammer::numTracks]{};
};

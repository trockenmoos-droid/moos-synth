#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmReverb.h>
#include <tm/TmVoices.h>

// SANKT WOLFGANG – Kirchenglocken und Pfeifenorgel
//  Glocke: modale Synthese mit den Teiltönen einer Kirchenglocke (Unterton/Hum, Prim, Terz, Quint, Nominal,
//          Oberquint, Oktave …), jeder Teilton als schwebendes Paar mit eigener Abklingzeit.
//  Orgel:  Register 16', 8', 4', 2 2/3', 2', 1 3/5' und Mixtur, Anblasgeräusch (Chiff), Wind, Tremulant.
namespace wolfgang
{
    constexpr int numBellPartials = 9;
    constexpr int numRanks = 7;
    // Teiltöne relativ zum Schlagton (Nominal) und ihre relative Abklingzeit / Lautstärke
    inline const float bellRatio[numBellPartials]  = { 0.25f, 0.5f, 0.6f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f };
    inline const float bellDecay[numBellPartials]  = { 1.0f, 0.75f, 0.62f, 0.42f, 0.36f, 0.24f, 0.17f, 0.11f, 0.08f };
    inline const float bellAmp[numBellPartials]    = { 0.45f, 0.6f, 0.85f, 0.3f, 1.0f, 0.55f, 0.35f, 0.2f, 0.12f };
    // Fußtonlagen: 16' 8' 4' 2 2/3' 2' 1 3/5' Mixtur(1' + 2/3')
    inline const float rankRatio[numRanks] = { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 8.0f };
    inline const char* rankIds[numRanks]   = { "r16", "r8", "r4", "r223", "r2", "r135", "mixtur" };
}

struct WolfgangParams
{
    float glocke, groesse, terz, material, schwebung, anschlag, daempfung;
    float ranks[wolfgang::numRanks], orgel, chiff, wind, tremulant;
    double sr;
    float tremPhaseInc;
};

class WolfgangVoice : public tmk::VoiceBase
{
public:
    void setup (double sampleRate, const WolfgangParams* p, juce::uint32 seed) { sr = sampleRate; params = p; rng.setSeed ((juce::int64) seed); }
    void start (bool wasActive);
    void release();
    void kill() { bellGain = 0.0f; organEnv.reset(); active = false; }
    void render (float* L, float* R, int n, const tmk::PlayContext& ctx);

    juce::ADSR organEnv;
    float organLevelNow = 0.0f, bellLevelNow = 0.0f;

private:
    double sr = 48000.0;
    const WolfgangParams* params = nullptr;
    juce::Random rng;

    // Glocke: Teilton-Paare
    double bPhase[wolfgang::numBellPartials][2] {};
    float  bAmp[wolfgang::numBellPartials] {}, bCoef[wolfgang::numBellPartials] {}, bDetune[wolfgang::numBellPartials] {};
    float  bInharm[wolfgang::numBellPartials] {};
    float  bellGain = 0.0f, releaseCoef = 1.0f;
    bool   damping = false;
    float  strikeNoise = 0.0f, strikeCoef = 0.0f;
    tmk::dsp::Svf strikeFilter;

    // Orgel
    double oPhase[wolfgang::numRanks] {};
    float  chiffEnv = 0.0f;
    tmk::dsp::Svf chiffFilter, windFilter;
    double tremPhase = 0.0;
};

class WolfgangProcessor : public tmk::Processor
{
public:
    WolfgangProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    juce::MidiKeyboardState keyboardState;
    std::atomic<float> bellLevel { 0.0f }, organLevel { 0.0f };

private:
    tmk::VoiceManager<WolfgangVoice, 16> voices;
    WolfgangParams wp {};
    tmk::FdnReverb reverb;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> gainSm;

    using AF = std::atomic<float>*;
    AF glocke{}, groesse{}, terz{}, material{}, schwebung{}, anschlag{}, daempfung{};
    AF ranks[wolfgang::numRanks]{}, orgel{}, chiff{}, wind{}, tremulant{}, oAttack{}, oRelease{};
    AF hall{}, hallZeit{}, volume{};
};

#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmReverb.h>
#include <tm/TmInstruments.h>

// RÖMER – Noricum: Hydraulis (Wasserorgel: der Luftdruck kommt aus einem Wasserkessel und schwankt,
// dazu Blubbern) und Cornu (großes, rundes Bronzehorn).
struct RoemerParams
{
    float hydraulis, wasserdruck, blubbern, luft, reihen, cornu, hell, anblasen;
    float pressure;          // aktueller Druck (gemeinsam für alle Pfeifen)
    double sr;
};

class RoemerVoice : public tmk::VoiceBase
{
public:
    void setup (double sampleRate, const RoemerParams* p, juce::uint32 seed) { sr = sampleRate; params = p; rng.setSeed ((juce::int64) seed); env.setSampleRate (sr); }
    void start (bool wasActive)
    {
        for (auto& pp : pipes) if (! wasActive) pp.start (rng);
        horn.start (-params->anblasen * 3.0f, wasActive, rng);
        env.noteOn();
    }
    void release() { env.noteOff(); }
    void kill() { env.reset(); active = false; }
    void render (float* L, float* R, int n, const tmk::PlayContext& ctx)
    {
        const auto& p = *params;
        const double f = hz (note + ctx.bend);
        tmk::inst::Horn::Settings hs;
        hs.hell = juce::jlimit (0.0f, 1.0f, p.hell + 0.4f * ctx.modWheel);
        hs.growl = 0.05f + 0.3f * juce::jmax (ctx.aftertouch, polyPressure);
        hs.zunge = 0.0f; hs.formantHz = 520.0f; hs.formantAmt = 0.25f;
        const int nRanks = 1 + (int) std::round (p.reihen * 2.0f);   // 1-3 Pfeifenreihen (8', 4', 2 2/3')
        const double rankRatio[3] = { 1.0, 2.0, 3.0 };
        const float rankAmp[3] = { 1.0f, 0.5f, 0.3f };
        const float pan = juce::jlimit (-0.6f, 0.6f, (note - 60) / 36.0f);
        const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
        for (int i = 0; i < n; ++i)
        {
            const float e = env.getNextSample();
            float y = 0.0f;
            if (p.hydraulis > 0.0f)
            {
                // Wasserdruck: Lautstärke und (leicht) Tonhöhe schwanken mit
                const double fp = f * (1.0 + 0.004 * p.pressure * p.wasserdruck);
                float org = 0.0f;
                for (int r = 0; r < nRanks; ++r)
                    org += pipes[r].process (fp * rankRatio[r], p.luft, sr, rng) * rankAmp[r];
                y += org * p.hydraulis * 0.16f * (1.0f + 0.35f * p.pressure * p.wasserdruck);
            }
            if (p.cornu > 0.0f)
                y += horn.process (f, e, hs, sr, rng) * p.cornu * 0.24f;
            y *= e * (0.45f + 0.55f * velocity);
            L[i] += y * gl; R[i] += y * gr;
        }
        if (! env.isActive()) active = false;
    }

    juce::ADSR env;

private:
    double sr = 48000.0;
    const RoemerParams* params = nullptr;
    juce::Random rng;
    tmk::inst::Pipe pipes[3];
    tmk::inst::Horn horn;
};

class RoemerProcessor : public tmk::Processor
{
public:
    RoemerProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    juce::MidiKeyboardState keyboardState;
    tmk::Scope scope;

private:
    tmk::VoiceManager<RoemerVoice, 12> voices;
    RoemerParams rp {};
    juce::Random rng;
    float pressTarget = 0.0f, press = 0.0f;
    tmk::dsp::Mode bubbles[4];
    int nextBubble = 0;
    float act = 0.0f;

    tmk::FdnReverb reverb;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> gainSm;

    using AF = std::atomic<float>*;
    AF hydraulis{}, wasserdruck{}, blubbern{}, luft{}, reihen{}, cornu{}, hell{}, anblasen{}, attack{}, release{}, hall{}, raum{}, volume{};
};

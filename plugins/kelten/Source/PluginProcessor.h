#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmReverb.h>
#include <tm/TmInstruments.h>

// KELTEN – Hallstattzeit: Carnyx (keltisches Kriegshorn mit Eberkopf-Schalltrichter und rasselnder Zunge),
// Bordun (Grundton + Quinte, unabhängig vom Spiel) und "Eisenzeit" (Lo-Fi: Erde, Rost, Alter).
namespace kelten
{
    inline const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
}

struct KeltenParams
{
    float carnyx, growl, zunge, anblasen, hell;
    float heer = 0.0f, zerstreuung = 0.3f;      // Heer: weitere Krieger blasen mit, leicht verstimmt und verspätet
    double sr;
};

class KeltenVoice : public tmk::VoiceBase
{
public:
    void setup (double sampleRate, const KeltenParams* p, juce::uint32 seed) { sr = sampleRate; params = p; rng.setSeed ((juce::int64) seed); env.setSampleRate (sr); }
    void start (bool wasActive)
    {
        horn.start (-params->anblasen * 5.0f, wasActive, rng);
        env.noteOn();
        // Heer: jeder Mitspieler mit eigener Verstimmung, Einsatz-Verzögerung und Platz im Stereobild
        const float z = params->zerstreuung;
        for (int k = 0; k < numWarriors; ++k)
        {
            auto& w = warriors[k];
            if (! wasActive) w.horn.start (-params->anblasen * 5.0f, false, rng);
            w.cents = (k % 2 == 0 ? 1.0f : -1.0f) * (4.0f + 18.0f * z) * (0.5f + 0.5f * rng.nextFloat());
            w.delay = (int) ((0.02f + 0.25f * z * rng.nextFloat() * (k + 1) / numWarriors) * (float) sr);
            w.join = wasActive ? w.join : 0.0f;
            w.pan = (k == 0 ? -0.8f : k == 1 ? 0.8f : (rng.nextFloat() * 1.2f - 0.6f));
            w.drift = 0.0f;
        }
    }
    void release() { env.noteOff(); }
    void kill() { env.reset(); active = false; }
    void render (float* L, float* R, int n, const tmk::PlayContext& ctx)
    {
        const auto& p = *params;
        tmk::inst::Horn::Settings s;
        s.hell = juce::jlimit (0.0f, 1.0f, p.hell + 0.4f * ctx.modWheel);
        s.growl = juce::jlimit (0.0f, 1.0f, p.growl + 0.6f * juce::jmax (ctx.aftertouch, polyPressure));
        s.zunge = p.zunge; s.formantHz = 850.0f; s.formantAmt = 0.45f;
        const double f = hz (note + ctx.bend);
        const float pan = juce::jlimit (-0.5f, 0.5f, (note - 55) / 40.0f);
        const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
        const float army = p.heer;
        const float norm = 1.0f / (1.0f + 0.9f * army);
        const float joinStep = 1.0f / (0.08f * (float) sr);
        float wgl[numWarriors], wgr[numWarriors];
        double wf[numWarriors];
        for (int k = 0; k < numWarriors; ++k)
        {
            auto& w = warriors[k];
            w.drift += (rng.nextFloat() - 0.5f) * 0.6f; w.drift *= 0.995f;            // langsam schwankende Intonation
            wf[k] = f * std::pow (2.0, (w.cents + w.drift) / 1200.0);
            wgl[k] = std::sqrt (0.5f * (1.0f - w.pan)); wgr[k] = std::sqrt (0.5f * (1.0f + w.pan));
        }
        for (int i = 0; i < n; ++i)
        {
            const float e = env.getNextSample();
            const float amp = e * p.carnyx * 0.28f * (0.4f + 0.6f * velocity);
            const float press = e * (0.5f + 0.5f * velocity);
            const float y = horn.process (f, press, s, sr, rng) * amp;
            float yl = y * gl, yr = y * gr;
            if (army > 0.001f)
                for (int k = 0; k < numWarriors; ++k)
                {
                    auto& w = warriors[k];
                    if (w.delay > 0) { --w.delay; continue; }
                    w.join = juce::jmin (1.0f, w.join + joinStep);
                    const float wy = w.horn.process (wf[k], press * w.join, s, sr, rng) * amp * w.join * army * (k < 2 ? 0.8f : 0.6f);
                    yl += wy * wgl[k]; yr += wy * wgr[k];
                }
            L[i] += yl * norm; R[i] += yr * norm;
        }
        if (! env.isActive()) active = false;
    }

    juce::ADSR env;

private:
    double sr = 48000.0;
    const KeltenParams* params = nullptr;
    juce::Random rng;
    tmk::inst::Horn horn;
    static constexpr int numWarriors = 3;
    struct Warrior { tmk::inst::Horn horn; float cents = 0.0f, join = 0.0f, pan = 0.0f, drift = 0.0f; int delay = 0; };
    Warrior warriors[numWarriors];
};

class KeltenProcessor : public tmk::Processor
{
public:
    KeltenProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    juce::MidiKeyboardState keyboardState;
    tmk::Scope scope;

private:
    tmk::VoiceManager<KeltenVoice, 12> voices;
    KeltenParams kp {};
    juce::Random rng;

    // Bordun
    double dronePhase[2] {};
    tmk::dsp::Svf droneLp[2];
    float droneEnv = 0.0f, droneDrift = 0.0f;

    // Eisenzeit
    std::vector<float> wowBuf[2];
    int wowW = 0;
    double wowPhase = 0.0;
    float hold[2] {}, ageLp[2] {}, rumble = 0.0f;
    int holdCount = 0;

    tmk::FdnReverb reverb;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> gainSm;

    using AF = std::atomic<float>*;
    AF carnyx{}, growl{}, zunge{}, anblasen{}, hell{}, attack{}, release{}, bordun{}, bordunTon{}, quinte{},
       erde{}, rost{}, alter{}, hall{}, volume{}, heer{}, zerstreuung{};
};

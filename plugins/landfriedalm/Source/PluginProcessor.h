#pragma once

#include <tm/TmEnvelope.h>
#include <tm/TmProcessor.h>
#include <tm/TmReverb.h>
#include <tm/TmVoices.h>

// LANDFRIEDALM – Alm-Instrumente: Zither und Hackbrett (physikalisch modellierte Saiten),
// Kuhglocke (modal), Jodler (Formant-Stimme mit Registerwechsel). Dazu ein Echo vom Berg.
namespace alm
{
    inline const juce::StringArray instruments { "Zither", "Hackbrett", "Kuhglocke", "Jodler" };
    enum { zither, hackbrett, kuhglocke, jodler };
    // Wirbel: gehaltene Töne werden schnell wiederholt angeschlagen (Hackbrett-Tremolo, Zither-Tremolo, schüttelnde Glocke)
    inline const juce::StringArray rollRates { "Aus", "1/8", "1/16", "1/16T", "1/32" };
    inline double rollBeats (int i) { const double v[] = { 0.0, 0.5, 0.25, 1.0 / 6.0, 0.125 }; return v[juce::jlimit (0, 4, i)]; }
}

struct AlmParams
{
    int instrument = 0;
    float hell, abklingen, koerper, schwebung, anschlag, vokal, jodelsprung;
    double sr;
};

// Karplus-Strong-Saite mit gebrochener Verzögerung, Dämpfung und Verlustfilter
struct KsString
{
    std::vector<float> buf;
    int w = 0;
    float delay = 100.0f, loss = 0.99f, lpCoef = 0.5f, lpState = 0.0f, ap = 0.0f;
    void prepare (double sr) { buf.assign ((size_t) (sr / 20.0) + 4, 0.0f); w = 0; lpState = 0.0f; }
    void tune (double freq, double sr, float decaySec, float bright)
    {
        delay = (float) juce::jlimit (2.0, (double) buf.size() - 3.0, sr / freq - 0.5);
        loss = (float) std::pow (10.0, -3.0 / (freq * juce::jmax (0.05f, decaySec)));
        lpCoef = 0.05f + 0.6f * (1.0f - bright);     // Saitenhelligkeit
    }
    void excite (const float* src, int n)
    {
        for (int i = 0; i < n && i < (int) buf.size(); ++i) buf[(size_t) ((w - (int) delay + i + (int) buf.size() * 4) % (int) buf.size())] += src[i];
    }
    inline float process() noexcept
    {
        const int size = (int) buf.size();
        float rp = (float) w - delay;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = juce::jlimit (0, size - 1, (int) rp);
        const int i1 = (i0 + 1) % size;
        const float fr = rp - (float) i0;
        const float y = buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
        lpState = y + lpCoef * (lpState - y);
        buf[(size_t) w] = lpState * loss;
        if (++w >= size) w = 0;
        return y;
    }
};

class AlmVoice : public tmk::VoiceBase
{
public:
    void setup (double sampleRate, const AlmParams* p, juce::uint32 seed);
    void start (bool wasActive);
    void release();
    void kill() { env.reset(); active = false; }
    void render (float* L, float* R, int n, const tmk::PlayContext& ctx);

    tmk::Adsr env;

private:
    double sr = 48000.0;
    const AlmParams* params = nullptr;
    juce::Random rng;
    int inst = 0;

    KsString str[2];
    std::vector<float> scratch;
    tmk::dsp::Svf body[3];
    float releaseGain = 1.0f, releaseCoef = 1.0f;
    bool releasing = false;
    int silentCount = 0;

    // Kuhglocke
    double mPhase[5][2] {};
    float mAmp[5] {}, mCoef[5] {}, clank = 0.0f;
    tmk::dsp::Svf clankFilter;

    // Jodler
    double gPhase = 0.0, vibPhase = 0.0;
    float pitchOffset = 0.0f, flipTimer = 0.0f, ageSec = 0.0f, headMix = 0.0f;
    tmk::dsp::Svf formant[3];
    float aspiration = 0.0f;
};

class AlmProcessor : public tmk::Processor
{
public:
    AlmProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    juce::MidiKeyboardState keyboardState;
    tmk::Scope scope;

private:
    tmk::VoiceManager<AlmVoice, 16> voices;
    AlmParams ap {};
    tmk::FdnReverb reverb;
    juce::AudioBuffer<float> wet;
    std::vector<float> echoBuf[2];
    int echoW = 0;
    float echoLp[2] {};
    juce::SmoothedValue<float> gainSm, echoTimeSm;
    int lastInstrument = -1;

    using AF = std::atomic<float>*;
    AF instrument{}, hell{}, abklingen{}, koerper{}, schwebung{}, anschlag{}, vokal{}, jodelsprung{},
       echo{}, echoZeit{}, hall{}, volume{}, wirbel{}, wirbelFrei{};
    // Wirbel
    void rollMidi (const juce::MidiBuffer& midi, int n);   // Ergebnis in rollBuf
    struct Held { int note; float vel; };
    std::vector<Held> held;
    int rollNext = 0, rollHand = 0;
    double untilRoll = 0.0;
    juce::MidiBuffer rollBuf;
    juce::Random rollRng;
public:
    std::atomic<int> rollStrikes { 0 };
};

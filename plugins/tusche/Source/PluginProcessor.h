#pragma once

#include <tm/TmProcessor.h>
#include <tm/TmVoices.h>
#include <tm/TmReverb.h>
#include <juce_audio_formats/juce_audio_formats.h>

// TUSCHE – Granular-Sampler. Ein Sample wird in 16 „Striche“ (Chops) geteilt; jeder Strich hat eigene
// Position, Korngröße, Dichte, Chaos, Raum und Tonhöhe. Drei Spielweisen:
//   HALTEN  – Taste spielt den gewählten Strich chromatisch und hält ihn (Latch), Akkorde möglich
//   SCHRITT – Sequenzer läuft im DAW-Takt durch die 16 Striche, eine Taste transponiert
//   TASTEN  – jede Taste (ab C3) spielt einen eigenen Strich
namespace tusche
{
    constexpr int numChops = 16;
    constexpr int maxVoices = 8;
    constexpr int maxGrains = 160;
    inline const juce::StringArray modes { "Halten", "Schritt", "Tasten" };
    inline const juce::StringArray stepLens { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    inline double stepBeats (int i) { const double v[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 }; return v[juce::jlimit (0, 5, i)]; }
    inline const juce::StringArray grainDivs { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64" };
    inline double divBeats (int i) { const double v[] = { 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 0.0625 }; return v[juce::jlimit (0, 8, i)]; }
    inline const juce::StringArray sources { "Klavier", "Glas", "Chor", "Holz", "Eigenes Sample" };
    inline const juce::StringArray scoreLens { "1 Takt", "2 Takte", "4 Takte", "8 Takte", "16 Takte" };
    inline double scoreBars (int i) { const double v[] = { 1, 2, 4, 8, 16 }; return v[juce::jlimit (0, 4, i)]; }
    constexpr int numSheets = 5;          // eingebaute Blätter; Index 5 = eigenes Bild
    constexpr int scoreCols = 256;
    // Parameter je Strich
    inline const char* chopParams[] = { "pos", "korn", "dichte", "chaos", "raum", "ton" };
    constexpr int numChopParams = 6;
    inline juce::String cid (int chop, const char* p) { return "c" + juce::String (chop + 1).paddedLeft ('0', 2) + p; }
}

class TuscheProcessor : public tmk::Processor
{
public:
    TuscheProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    struct Source { juce::AudioBuffer<float> data; double rate = 48000.0; juce::String name; juce::File file; };
    // Message-Thread
    juce::String loadSample (const juce::File&);
    void clearSample();
    static bool isAudioFile (const juce::File&);
    const Source* currentSource() const;            // was gerade klingt (für die Anzeige)
    std::atomic<int> sourceChanges { 0 };

    juce::MidiKeyboardState keyboardState;

    // --- Partitur: ein Blatt wird als Notenbild gelesen ---
    // je Spalte: Schwärze (0..1), Schwerpunkt der Tusche (0 oben .. 1 unten), Rauheit (zerrissene Ränder)
    struct Sheet { juce::Image image; juce::String name; juce::File file; float dark[tusche::scoreCols] {}, centre[tusche::scoreCols] {}, rough[tusche::scoreCols] {}; };
    static std::unique_ptr<Sheet> analyseSheet (const juce::Image&, const juce::String& name);
    const Sheet* sheet (int index) const;                       // 0..4 eingebaut, 5 eigenes (oder nullptr)
    int  currentSheetIndex() const { return sheetIndex.load(); }
    void setSheet (int index);                                  // Message-Thread
    juce::String loadSheetImage (const juce::File&);            // eigenes Bild (jpg/png)
    static bool isImageFile (const juce::File&);
    std::atomic<float> readHead { -1.0f };                       // 0..1, -1 = aus
    std::atomic<int> sheetChanges { 0 };

    // Anzeige: Körner (Position 0..1 im Sample, Leuchten), aktueller Schritt, aktive Striche
    struct GrainView { std::atomic<float> pos { 0 }, glow { 0 }, pan { 0 }; };
    GrainView grainView[tusche::maxGrains];
    std::atomic<int> currentStep { -1 };
    std::atomic<float> chopGlow[tusche::numChops] {};

private:
    struct Voice
    {
        bool active = false, keyDown = false;
        int note = 60, chop = 0;
        float vel = 1.0f, pitch = 0.0f;
        double untilGrain = 0.0, scan = 0.0;
        juce::ADSR env;
        float envNow = 0.0f;
        int gateLeft = -1;          // Schritt-Modus: Samples bis zum Loslassen
    };
    struct Grain
    {
        bool active = false;
        int voice = 0, age = 0, len = 1;
        double pos = 0.0, step = 1.0;
        float gl = 0.0f, gr = 0.0f, amp = 1.0f, send = 0.0f;
        float viewPos = 0.0f;
    };

    void noteOn (int note, float vel);
    void noteOff (int note);
    int  freeVoice();
    void spawnGrain (int voiceIndex, const Source& src, double intervalSamples);
    struct ScoreNow { bool on = false; float dark = 1.0f, centre = 0.5f, rough = 0.0f; };
    ScoreNow scoreAt (double beat) const;
    ScoreNow scoreNow;
    float chopValue (int chop, int param) const { return chopP[chop][param]->load(); }
    void stateRestored() override;
    void setUserSource (std::unique_ptr<Source>);
    static std::unique_ptr<Source> makeBuiltIn (int which);

    double sr = 48000.0;
    juce::Random rng;
    Voice voices[tusche::maxVoices];
    Grain grains[tusche::maxGrains];
    int keysHeld = 0, lastNote = 60;
    std::vector<int> heldNotes;
    double freeBeat = 0.0;
    long long lastStepIndex = -1;
    juce::ADSR::Parameters adsr;

    tmk::dsp::Svf lp[2], hp[2];
    tmk::FdnReverb reverb;
    juce::AudioBuffer<float> sendBuf;
    juce::SmoothedValue<float> outSm;

    std::unique_ptr<Source> builtIn[4];
    std::unique_ptr<Source> userOwner;
    std::atomic<const Source*> userSrc { nullptr };
    struct Retired { std::unique_ptr<Source> s; juce::uint32 at; };
    std::vector<Retired> retired;
    juce::AudioFormatManager formats;
    std::unique_ptr<Sheet> sheets[tusche::numSheets];
    std::unique_ptr<Sheet> userSheetOwner;
    std::atomic<const Sheet*> userSheet { nullptr };
    std::vector<std::unique_ptr<Sheet>> retiredSheets;
    std::atomic<int> sheetIndex { 0 };

    using AF = std::atomic<float>*;
    AF modus{}, sync{}, schritt{}, auswahl{}, quelle{}, wandern{}, kornteilung{};
    AF attack{}, decay{}, sustain{}, release{};
    AF tief{}, hoch{}, reso{}, raumGroesse{}, ausgang{};
    AF partitur{}, takte{}, einfluss{};
    AF chopP[tusche::numChops][tusche::numChopParams] {};
    AF chopOn[tusche::numChops] {};
};

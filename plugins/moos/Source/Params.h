#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace MoosParams
{
    inline const juce::StringArray lfoShapes   { "Sine", "Triangle", "Saw", "Square", "S&H" };
    inline const juce::StringArray modSources  { "Off", "LFO 1", "LFO 2", "Mod Wheel", "Aftertouch",
                                                 "Velocity", "Filter Env", "Note Random", "Key Track" };
    inline const juce::StringArray modDests    { "Pos A", "Pos B", "Cutoff", "Resonance", "Pitch",
                                                 "Fine B", "Level A", "Level B", "Uni Detune", "Volume" };
    inline const juce::StringArray arpModes    { "Up", "Down", "Up/Down", "Random", "As Played" };
    inline const juce::StringArray arpRates    { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    inline const juce::StringArray delayDivs   { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2" };

    inline double arpRateBeats (int i)   { const double v[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 }; return v[juce::jlimit (0, 5, i)]; }
    inline double delayDivBeats (int i)  { const double v[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 }; return v[juce::jlimit (0, 7, i)]; }

    enum ModSrc { srcOff, srcLfo1, srcLfo2, srcModWheel, srcAftertouch, srcVelocity, srcFiltEnv, srcRandom, srcKey };
    enum ModDst { dstPosA, dstPosB, dstCutoff, dstReso, dstPitch, dstFineB, dstLevelA, dstLevelB, dstUniDetune, dstVolume, numDests };

    constexpr int numModSlots = 4;

    inline juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using P  = juce::AudioParameterFloat;
        using PI = juce::AudioParameterInt;
        using PC = juce::AudioParameterChoice;
        using PB = juce::AudioParameterBool;
        using ID = juce::ParameterID;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

        // --- Version 1 (IDs bleiben kompatibel) ---
        p.push_back (std::make_unique<P>  (ID { "posA", 1 },   "Osc A Position", 0.0f, 1.0f, 0.3f));
        p.push_back (std::make_unique<PI> (ID { "octA", 1 },   "Osc A Octave", -2, 2, 0));
        p.push_back (std::make_unique<P>  (ID { "levelA", 1 }, "Osc A Level", 0.0f, 1.0f, 0.8f));
        p.push_back (std::make_unique<P>  (ID { "posB", 1 },   "Osc B Position", 0.0f, 1.0f, 0.6f));
        p.push_back (std::make_unique<PI> (ID { "octB", 1 },   "Osc B Octave", -2, 2, -1));
        p.push_back (std::make_unique<P>  (ID { "fineB", 1 },  "Osc B Fine (ct)", -100.0f, 100.0f, 7.0f));
        p.push_back (std::make_unique<P>  (ID { "levelB", 1 }, "Osc B Level", 0.0f, 1.0f, 0.5f));
        p.push_back (std::make_unique<PI> (ID { "uniVoices", 1 }, "Unison Voices", 1, 7, 5));
        p.push_back (std::make_unique<P>  (ID { "uniDetune", 1 }, "Unison Detune (ct)", 0.0f, 60.0f, 14.0f));
        p.push_back (std::make_unique<P>  (ID { "uniWidth", 1 },  "Unison Width", 0.0f, 1.0f, 0.85f));
        p.push_back (std::make_unique<P> (ID { "cutoff", 1 }, "Cutoff (Hz)", skewed (20.0f, 20000.0f, 1200.0f), 1800.0f));
        p.push_back (std::make_unique<P> (ID { "reso", 1 },   "Resonance", 0.0f, 1.0f, 0.25f));
        p.push_back (std::make_unique<P> (ID { "envAmt", 1 }, "Filter Env Amount", -1.0f, 1.0f, 0.3f));
        p.push_back (std::make_unique<P> (ID { "fA", 1 }, "Filter Attack (s)",  skewed (0.001f, 10.0f, 1.0f), 1.5f));
        p.push_back (std::make_unique<P> (ID { "fD", 1 }, "Filter Decay (s)",   skewed (0.001f, 10.0f, 1.0f), 2.5f));
        p.push_back (std::make_unique<P> (ID { "fS", 1 }, "Filter Sustain",     0.0f, 1.0f, 0.4f));
        p.push_back (std::make_unique<P> (ID { "fR", 1 }, "Filter Release (s)", skewed (0.001f, 15.0f, 1.5f), 3.0f));
        p.push_back (std::make_unique<P> (ID { "aA", 1 }, "Amp Attack (s)",  skewed (0.001f, 10.0f, 1.0f), 0.8f));
        p.push_back (std::make_unique<P> (ID { "aD", 1 }, "Amp Decay (s)",   skewed (0.001f, 10.0f, 1.0f), 1.0f));
        p.push_back (std::make_unique<P> (ID { "aS", 1 }, "Amp Sustain",     0.0f, 1.0f, 0.85f));
        p.push_back (std::make_unique<P> (ID { "aR", 1 }, "Amp Release (s)", skewed (0.001f, 15.0f, 1.5f), 3.5f));
        p.push_back (std::make_unique<P> (ID { "lfoRate", 1 }, "LFO 1 Rate (Hz)", skewed (0.01f, 10.0f, 0.5f), 0.12f));
        p.push_back (std::make_unique<P> (ID { "lfoPos", 1 },  "LFO 1 -> Position", 0.0f, 1.0f, 0.35f));
        p.push_back (std::make_unique<P> (ID { "lfoCut", 1 },  "LFO 1 -> Cutoff", 0.0f, 1.0f, 0.15f));
        p.push_back (std::make_unique<P> (ID { "chorusMix", 1 },  "Chorus Mix", 0.0f, 1.0f, 0.35f));
        p.push_back (std::make_unique<P> (ID { "reverbSize", 1 }, "Reverb Size", 0.0f, 1.0f, 0.85f));
        p.push_back (std::make_unique<P> (ID { "reverbMix", 1 },  "Reverb Mix", 0.0f, 1.0f, 0.4f));
        p.push_back (std::make_unique<P> (ID { "volume", 1 },     "Volume (dB)", -48.0f, 6.0f, -6.0f));

        // --- Version 2 ---
        p.push_back (std::make_unique<PC> (ID { "lfoShape", 2 },  "LFO 1 Shape", lfoShapes, 0));
        p.push_back (std::make_unique<P>  (ID { "lfo2Rate", 2 },  "LFO 2 Rate (Hz)", skewed (0.01f, 20.0f, 1.0f), 2.0f));
        p.push_back (std::make_unique<PC> (ID { "lfo2Shape", 2 }, "LFO 2 Shape", lfoShapes, 1));

        const int defSrc[] = { srcModWheel, srcAftertouch, srcLfo2, srcOff };
        const int defDst[] = { dstPosA, dstCutoff, dstPitch, dstPosB };
        const float defAmt[] = { 0.5f, 0.4f, 0.0f, 0.0f };
        for (int i = 0; i < numModSlots; ++i)
        {
            const auto n = juce::String (i + 1);
            p.push_back (std::make_unique<PC> (ID { "mod" + n + "Src", 2 }, "Mod " + n + " Source", modSources, defSrc[i]));
            p.push_back (std::make_unique<PC> (ID { "mod" + n + "Dst", 2 }, "Mod " + n + " Destination", modDests, defDst[i]));
            p.push_back (std::make_unique<P>  (ID { "mod" + n + "Amt", 2 }, "Mod " + n + " Amount", -1.0f, 1.0f, defAmt[i]));
        }

        p.push_back (std::make_unique<P>  (ID { "hpf", 2 },       "High-Pass (Hz)", skewed (20.0f, 2000.0f, 200.0f), 20.0f));
        p.push_back (std::make_unique<P>  (ID { "drift", 2 },     "Analog Drift", 0.0f, 1.0f, 0.25f));
        p.push_back (std::make_unique<P>  (ID { "glide", 2 },     "Glide (s)", skewed (0.0f, 3.0f, 0.3f), 0.0f));
        p.push_back (std::make_unique<PB> (ID { "mono", 2 },      "Mono / Legato", false));
        p.push_back (std::make_unique<PI> (ID { "bendRange", 2 }, "Pitch Bend Range", 0, 12, 2));

        p.push_back (std::make_unique<PB> (ID { "arpOn", 2 },   "Arp On", false));
        p.push_back (std::make_unique<PC> (ID { "arpMode", 2 }, "Arp Mode", arpModes, 0));
        p.push_back (std::make_unique<PC> (ID { "arpRate", 2 }, "Arp Rate", arpRates, 3));
        p.push_back (std::make_unique<PI> (ID { "arpOct", 2 },  "Arp Octaves", 1, 4, 1));
        p.push_back (std::make_unique<P>  (ID { "arpGate", 2 }, "Arp Gate", 0.05f, 1.0f, 0.6f));

        p.push_back (std::make_unique<PB> (ID { "delaySync", 2 }, "Delay Sync", true));
        p.push_back (std::make_unique<PC> (ID { "delayDiv", 2 },  "Delay Division", delayDivs, 3));
        p.push_back (std::make_unique<P>  (ID { "delayTime", 2 }, "Delay Time (ms)", skewed (10.0f, 1500.0f, 300.0f), 375.0f));
        p.push_back (std::make_unique<P>  (ID { "delayFb", 2 },   "Delay Feedback", 0.0f, 0.95f, 0.45f));
        p.push_back (std::make_unique<P>  (ID { "delayMix", 2 },  "Delay Mix", 0.0f, 1.0f, 0.0f));
        p.push_back (std::make_unique<P>  (ID { "shimmer", 2 },   "Shimmer", 0.0f, 1.0f, 0.0f));

        return { p.begin(), p.end() };
    }
}

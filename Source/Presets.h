#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Werkspresets: jeweils nur die Abweichungen von den Standardwerten.
// Werte in "echten" Einheiten (Hz, s, dB, Index bei Auswahllisten).
namespace MoosPresets
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    inline const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> list = {
            { "Init Moos", {} },

            { "Nebelwand", {
                { "posA", 0.28f }, { "posB", 0.58f }, { "octB", -1 }, { "fineB", 9.0f },
                { "uniVoices", 7 }, { "uniDetune", 18.0f }, { "uniWidth", 1.0f },
                { "cutoff", 1400.0f }, { "reso", 0.2f }, { "envAmt", 0.25f },
                { "aA", 2.5f }, { "aR", 5.0f }, { "fA", 3.0f },
                { "lfoRate", 0.07f }, { "lfoPos", 0.45f },
                { "reverbSize", 0.92f }, { "reverbMix", 0.55f }, { "shimmer", 0.35f }, { "drift", 0.35f } } },

            { "Glasregen", {
                { "posA", 0.86f }, { "posB", 0.72f }, { "octA", 1 }, { "octB", 0 }, { "levelB", 0.35f },
                { "uniVoices", 3 }, { "uniDetune", 8.0f },
                { "cutoff", 5200.0f }, { "envAmt", 0.0f },
                { "aA", 0.005f }, { "aD", 0.6f }, { "aS", 0.25f }, { "aR", 1.2f },
                { "arpOn", 1 }, { "arpMode", 2 }, { "arpRate", 3 }, { "arpOct", 2 }, { "arpGate", 0.4f },
                { "delayMix", 0.35f }, { "delayDiv", 3 }, { "delayFb", 0.55f },
                { "reverbMix", 0.45f }, { "shimmer", 0.25f } } },

            { "Dunkler Drone", {
                { "posA", 0.32f }, { "posB", 0.95f }, { "octA", -1 }, { "octB", -2 }, { "levelB", 0.7f },
                { "uniVoices", 5 }, { "uniDetune", 10.0f },
                { "cutoff", 380.0f }, { "reso", 0.45f }, { "envAmt", 0.15f },
                { "aA", 4.0f }, { "aR", 8.0f },
                { "lfoRate", 0.03f }, { "lfoPos", 0.6f }, { "lfoCut", 0.35f },
                { "mod3Src", 2 }, { "mod3Dst", 4 }, { "mod3Amt", 0.08f }, { "lfo2Rate", 0.2f }, { "lfo2Shape", 1 },
                { "drift", 0.6f }, { "reverbSize", 0.97f }, { "reverbMix", 0.6f }, { "volume", -4.0f } } },

            { "Hallstatt Morgen", {
                { "posA", 0.71f }, { "posB", 0.0f }, { "octB", 1 }, { "levelB", 0.3f },
                { "uniVoices", 3 }, { "uniDetune", 6.0f },
                { "cutoff", 2600.0f }, { "envAmt", 0.1f },
                { "aA", 1.2f }, { "aR", 4.0f },
                { "lfoPos", 0.15f }, { "chorusMix", 0.6f }, { "reverbMix", 0.5f }, { "shimmer", 0.15f } } },

            { "Trockenmoos", {
                { "posA", 1.0f }, { "posB", 0.62f }, { "levelB", 0.45f },
                { "uniVoices", 5 }, { "uniDetune", 20.0f },
                { "hpf", 220.0f }, { "cutoff", 2400.0f }, { "reso", 0.35f },
                { "aA", 1.5f }, { "aR", 4.0f },
                { "lfo2Rate", 3.5f }, { "lfo2Shape", 4 },
                { "mod3Src", 2 }, { "mod3Dst", 2 }, { "mod3Amt", 0.25f },
                { "drift", 0.7f }, { "delayMix", 0.2f }, { "delayDiv", 1 }, { "reverbMix", 0.45f } } },

            { "Salzkammergut Strings", {
                { "posA", 0.29f }, { "posB", 0.29f }, { "octB", 0 }, { "fineB", -12.0f }, { "levelB", 0.7f },
                { "uniVoices", 7 }, { "uniDetune", 22.0f }, { "uniWidth", 1.0f },
                { "cutoff", 3200.0f }, { "reso", 0.1f }, { "envAmt", 0.2f }, { "fA", 0.6f },
                { "aA", 0.5f }, { "aD", 1.0f }, { "aS", 0.9f }, { "aR", 1.8f },
                { "lfoPos", 0.05f }, { "chorusMix", 0.45f }, { "reverbMix", 0.35f }, { "hpf", 90.0f } } },

            { "Moos Bass Mono", {
                { "posA", 0.29f }, { "posB", 0.43f }, { "octA", -1 }, { "octB", -1 }, { "fineB", 6.0f }, { "levelB", 0.7f },
                { "uniVoices", 2 }, { "uniDetune", 9.0f }, { "uniWidth", 0.3f },
                { "cutoff", 260.0f }, { "reso", 0.5f }, { "envAmt", 0.55f },
                { "fA", 0.003f }, { "fD", 0.35f }, { "fS", 0.1f }, { "fR", 0.2f },
                { "aA", 0.003f }, { "aD", 0.4f }, { "aS", 0.8f }, { "aR", 0.15f },
                { "lfoPos", 0.0f }, { "lfoCut", 0.0f },
                { "mono", 1 }, { "glide", 0.09f },
                { "chorusMix", 0.0f }, { "reverbMix", 0.08f }, { "drift", 0.15f }, { "volume", -3.0f } } },

            { "Atemluft", {
                { "posA", 0.55f }, { "posB", 0.15f }, { "levelB", 0.6f },
                { "uniVoices", 5 }, { "uniDetune", 12.0f },
                { "cutoff", 900.0f }, { "envAmt", 0.0f },
                { "aA", 1.8f }, { "aR", 4.5f },
                { "mod1Src", 3 }, { "mod1Dst", 0 }, { "mod1Amt", 0.45f },
                { "mod2Src", 4 }, { "mod2Dst", 2 }, { "mod2Amt", 0.5f },
                { "mod3Src", 2 }, { "mod3Dst", 4 }, { "mod3Amt", 0.07f }, { "lfo2Rate", 5.0f }, { "lfo2Shape", 0 },
                { "mod4Src", 3 }, { "mod4Dst", 2 }, { "mod4Amt", 0.35f },
                { "reverbMix", 0.5f }, { "shimmer", 0.2f } } },

            { "Bodennebel Arp", {
                { "posA", 0.43f }, { "posB", 0.14f }, { "octB", 0 }, { "levelB", 0.4f },
                { "uniVoices", 3 }, { "uniDetune", 10.0f },
                { "cutoff", 1100.0f }, { "reso", 0.4f }, { "envAmt", 0.45f },
                { "fA", 0.002f }, { "fD", 0.3f }, { "fS", 0.0f },
                { "aA", 0.003f }, { "aD", 0.5f }, { "aS", 0.3f }, { "aR", 0.6f },
                { "lfoCut", 0.4f }, { "lfoRate", 0.18f },
                { "arpOn", 1 }, { "arpMode", 2 }, { "arpRate", 2 }, { "arpOct", 2 }, { "arpGate", 0.5f },
                { "delayMix", 0.3f }, { "delayDiv", 5 }, { "delayFb", 0.5f }, { "reverbMix", 0.4f } } },
        };
        return list;
    }

    // setzt alle Parameter auf Standard und wendet dann das Preset an
    inline void apply (juce::AudioProcessorValueTreeState& s, const Preset& p)
    {
        for (auto* param : s.processor.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (param))
                r->setValueNotifyingHost (r->getDefaultValue());

        for (auto& [id, value] : p.values)
            if (auto* r = s.getParameter (id))
                r->setValueNotifyingHost (r->convertTo0to1 (value));
    }

    inline juce::File userFolder()
    {
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("trockenmoos").getChildFile ("MOOS Presets");
        f.createDirectory();
        return f;
    }

    inline juce::Array<juce::File> userPresets()
    {
        auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.moospreset");
        files.sort();
        return files;
    }
}

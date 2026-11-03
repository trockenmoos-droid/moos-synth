#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TmTextures.h"

namespace tmk
{
    // Werkspreset: nur Abweichungen vom Standard, Werte in echten Einheiten
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    // Gemeinsame Basis: Parameter, Werks- und eigene Presets, Zustand speichern/laden
    class Processor : public juce::AudioProcessor
    {
    public:
        Processor (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout,
                   juce::String pluginName, std::vector<Preset> factoryPresets)
            : AudioProcessor (buses),
              apvts (*this, nullptr, juce::Identifier (pluginName.removeCharacters (" ").toUpperCase()), std::move (layout)),
              name (std::move (pluginName)), factory (std::move (factoryPresets))
        {
            apvts.state.setProperty ("presetName", factory.empty() ? juce::String ("Init") : juce::String::fromUTF8 (factory.front().name), nullptr);
        }

        const juce::String getName() const override { return name; }
        bool hasEditor() const override { return true; }

        int getNumPrograms() override { return juce::jmax (1, (int) factory.size()); }
        int getCurrentProgram() override { return currentProgram; }
        void setCurrentProgram (int i) override { loadFactoryPreset (i); }
        const juce::String getProgramName (int i) override
        {
            return juce::isPositiveAndBelow (i, (int) factory.size()) ? juce::String::fromUTF8 (factory[(size_t) i].name) : juce::String();
        }
        void changeProgramName (int, const juce::String&) override {}

        const std::vector<Preset>& getFactoryPresets() const { return factory; }

        void loadFactoryPreset (int index)
        {
            if (! juce::isPositiveAndBelow (index, (int) factory.size())) return;
            currentProgram = index;
            resetToDefaults();
            for (auto& [id, value] : factory[(size_t) index].values)
                if (auto* r = apvts.getParameter (id))
                    r->setValueNotifyingHost (r->convertTo0to1 (value));
            apvts.state.setProperty ("presetName", juce::String::fromUTF8 (factory[(size_t) index].name), nullptr);
            ++presetChangeCounter;
        }

        juce::File userPresetFolder() const
        {
            auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("trockenmoos").getChildFile (name + " Presets");
            f.createDirectory();
            return f;
        }

        juce::Array<juce::File> userPresets() const
        {
            auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.tmpreset");
            files.sort();
            return files;
        }

        juce::String loadUserPreset (const juce::File& file)
        {
            auto xml = juce::XmlDocument::parse (file);
            if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
                return "Keine gültige " + name + "-Preset-Datei";
            resetToDefaults();
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
            stateRestored();
            ++presetChangeCounter;
            return {};
        }

        bool saveUserPreset (const juce::File& file)
        {
            apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
            ++presetChangeCounter;
            if (auto xml = apvts.copyState().createXml()) return xml->writeTo (file);
            return false;
        }

        juce::String getPresetName() const { return apvts.state.getProperty ("presetName", "Init").toString(); }

        void getStateInformation (juce::MemoryBlock& dest) override
        {
            if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
        }

        void setStateInformation (const void* data, int size) override
        {
            if (auto xml = getXmlFromBinary (data, size))
                if (xml->hasTagName (apvts.state.getType()))
                {
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    stateRestored();
                    ++presetChangeCounter;
                }
        }

        juce::AudioProcessorValueTreeState apvts;
        std::atomic<int> presetChangeCounter { 0 };

        // Host-Tempo (für Sync), vom Audio-Thread aktualisiert
        double readBpm()
        {
            if (auto* ph = getPlayHead())
                if (auto pos = ph->getPosition())
                    if (auto b = pos->getBpm()) return *b;
            return 120.0;
        }

    protected:
        virtual void stateRestored() {}

        void resetToDefaults()
        {
            for (auto* p : getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    r->setValueNotifyingHost (r->getDefaultValue());
        }

        const juce::String name;
        std::vector<Preset> factory;
        int currentProgram = 0;
        juce::SharedResourcePointer<tex::Service> textures;   // Textur-Erzeugung lebt so lange wie das Plugin
    };

    inline juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
}

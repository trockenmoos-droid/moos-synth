#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <tm/TmTextures.h>

namespace tt
{
    inline int failures = 0;

    inline void check (bool ok, const juce::String& what)
    {
        std::printf ("%s  %s\n", ok ? "OK  " : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }

    inline void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
    {
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr,
                                                    (unsigned) b.getNumChannels(), 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    inline void writePng (const juce::File& f, const juce::Image& img)
    {
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }

    inline bool finite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i))) return false;
        return true;
    }

    inline float peak (const juce::AudioBuffer<float>& b, int start, int num)
    {
        float p = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch) p = juce::jmax (p, b.getMagnitude (ch, start, num));
        return p;
    }

    // Verarbeitet "in" blockweise durch den Prozessor (Effekt) bzw. rendert mit MIDI (Synth)
    inline juce::AudioBuffer<float> run (juce::AudioProcessor& p, const juce::AudioBuffer<float>& in, double sr,
                                         std::function<void (int pos, juce::MidiBuffer&)> midiAt = {}, int block = 512)
    {
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        juce::AudioBuffer<float> out (2, in.getNumSamples());
        out.clear();
        juce::AudioBuffer<float> buf (2, block);
        for (int pos = 0; pos + block <= in.getNumSamples(); pos += block)
        {
            for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, in, juce::jmin (ch, in.getNumChannels() - 1), pos, block);
            juce::MidiBuffer midi;
            if (midiAt) midiAt (pos, midi);
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, block);
        }
        return out;
    }

    // Testsignal: kurze gezupfte Töne (Pentatonik), damit man Hall/Delay gut hört
    inline juce::AudioBuffer<float> plucks (double sr, double seconds, double activeSeconds)
    {
        juce::AudioBuffer<float> b (2, (int) (sr * seconds));
        b.clear();
        const int notes[] = { 62, 69, 66, 74, 71, 69, 64, 62 };
        const double step = activeSeconds / 8.0;
        for (int k = 0; k < 8; ++k)
        {
            const double f = 440.0 * std::pow (2.0, (notes[k] - 69) / 12.0);
            const int start = (int) (k * step * sr);
            for (int i = 0; i < (int) (sr * 1.2) && start + i < b.getNumSamples(); ++i)
            {
                const double t = i / sr;
                const float v = (float) (0.35 * std::exp (-t * 4.0) * (std::sin (2 * juce::MathConstants<double>::pi * f * t)
                                          + 0.4 * std::sin (2 * juce::MathConstants<double>::pi * f * 2.0 * t) * std::exp (-t * 6.0)
                                          + 0.15 * std::sin (2 * juce::MathConstants<double>::pi * f * 3.0 * t) * std::exp (-t * 9.0)));
                b.addSample (0, start + i, v * (k % 2 ? 0.8f : 1.0f));
                b.addSample (1, start + i, v * (k % 2 ? 1.0f : 0.8f));
            }
        }
        return b;
    }

    inline void waitForTexture (const juce::String& name, std::function<juce::Image()> gen, const juce::File& dumpDir = {})
    {
        juce::SharedResourcePointer<tmk::tex::Service> s;
        juce::Image a, b;
        for (int i = 0; i < 600 && ! s->get (name, gen, a, b); ++i) juce::Thread::sleep (50);
        if (dumpDir != juce::File() && a.isValid())
            writePng (dumpDir.getChildFile (name + "_texture.png"), a.rescaled (900, 585, juce::Graphics::highResamplingQuality));
    }

    // Editor-Schnappschuss (alle Seiten), Timer einmal laufen lassen, damit die Textur geladen wird
    template <typename EditorType>
    inline void screenshots (juce::AudioProcessor& p, const juce::File& dir, const juce::String& base, int pages)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        for (int page = 0; page < juce::jmax (1, pages); ++page)
        {
            if (auto* e = dynamic_cast<EditorType*> (ed.get())) e->showPage (page);
            writePng (dir.getChildFile (base + (pages > 1 ? juce::String (page) : juce::String()) + ".png"),
                      ed->createComponentSnapshot (ed->getLocalBounds()));
        }
    }
}

namespace tt
{
    // Standard-Testlauf für Synths: alle Presets, Stresstest, Hörprobe, Screenshot
    template <typename EditorType>
    inline void synthSuite (juce::AudioProcessor& proc, const juce::File& dir, const juce::String& name,
                            const juce::String& texKey, std::vector<int> demoPresets,
                            std::vector<int> chord = { 48, 55, 60, 64, 67 }, double holdSec = 4.0, double totalSec = 7.0)
    {
        const double sr = 48000.0;
        auto play = [&] (double secs, double rel, const std::vector<int>& notes, float vel)
        {
            juce::AudioBuffer<float> silence (2, (int) (secs * sr));
            silence.clear();
            return run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m)
            {
                if (pos == 0) for (int nn : notes) m.addEvent (juce::MidiMessage::noteOn (1, nn, vel), 0);
                const int r = (int) (rel * sr);
                if (pos <= r && pos + 512 > r) for (int nn : notes) m.addEvent (juce::MidiMessage::noteOff (1, nn), r - pos);
            });
        };

        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            auto out = play (totalSec, holdSec, chord, 0.8f);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            const float lvl = peak (out, (int) (sr * 0.3), (int) (sr * 2.5));
            check (finite (out) && lvl > 0.01f && lvl < 1.0f,
                   "Preset " + proc.getProgramName (i) + juce::String::formatted ("  Pegel=%.3f  Rechenzeit=%.1f%% Echtzeit (%d Toene)",
                                                                                lvl, ms / (totalSec * 10.0), (int) chord.size()));
        }

        // Stress: alle Regler auf Maximum, 16 Töne, schnelle Wiederholungen
        proc.setCurrentProgram (0);
        for (auto* p : proc.getParameters()) p->setValueNotifyingHost (1.0f);
        {
            std::vector<int> many;
            for (int k = 0; k < 16; ++k) many.push_back (36 + k * 4);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            juce::AudioBuffer<float> silence (2, (int) (6 * sr));
            silence.clear();
            auto out = run (proc, silence, sr, [&] (int pos, juce::MidiBuffer& m)
            {
                if (pos % 24576 == 0) for (int nn : many) m.addEvent (juce::MidiMessage::noteOn (1, nn, 1.0f), 0);
                if (pos % 24576 == 12288) for (int nn : many) m.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
            });
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            check (finite (out) && peak (out, 0, out.getNumSamples()) <= 1.0f,
                   juce::String::formatted ("Maximum, 16 Toene: Spitze=%.3f  Rechenzeit=%.0f%% Echtzeit", peak (out, 0, out.getNumSamples()), ms / 60.0));
        }

        // Hörprobe
        {
            const int per = (int) (totalSec * sr);
            juce::AudioBuffer<float> demo (2, per * (int) demoPresets.size());
            demo.clear();
            int at = 0;
            for (int pi : demoPresets)
            {
                proc.setCurrentProgram (pi);
                auto out = play (totalSec, holdSec, chord, 0.8f);
                for (int ch = 0; ch < 2; ++ch)
                {
                    demo.copyFrom (ch, at, out, ch, 0, per);
                    demo.applyGainRamp (ch, at + per - 12000, 12000, 1.0f, 0.0f);
                }
                at += per;
            }
            writeWav (dir.getChildFile (name + "_demo.wav"), demo, sr);
        }

        proc.setCurrentProgram (demoPresets.empty() ? 0 : demoPresets.front());
        play (2.0, 1.9, chord, 0.8f);
        waitForTexture (texKey, tmk::tex::landscapeFor (texKey), dir);
        screenshots<EditorType> (proc, dir, name, 1);
    }
}

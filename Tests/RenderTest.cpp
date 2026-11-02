// Offline-Test: spielt einen Akkord durch MOOS und schreibt render.wav
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/MossBackground.h"
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int block = 512;
    const int seconds = 14;

    MoosProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    juce::AudioBuffer<float> out (2, (int) (sr * seconds));
    out.clear();
    juce::AudioBuffer<float> buf (2, block);

    // Akkord: Am9 (A2, E3, G3, B3, C4), dann Fmaj7, danach Release
    const int chord1[] = { 45, 52, 55, 59, 60 };
    const int chord2[] = { 41, 48, 52, 55, 57 };

    for (int pos = 0; pos + block <= out.getNumSamples(); pos += block)
    {
        juce::MidiBuffer midi;
        auto at = [&] (double t) { return (int) (t * sr) - pos; };
        auto inBlock = [&] (double t) { int o = at (t); return o >= 0 && o < block ? o : -1; };

        if (int o = inBlock (0.0); o >= 0) for (int n : chord1) midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), o);
        if (int o = inBlock (4.0); o >= 0) for (int n : chord1) midi.addEvent (juce::MidiMessage::noteOff (1, n), o);
        if (int o = inBlock (4.0); o >= 0) for (int n : chord2) midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), o);
        if (int o = inBlock (8.0); o >= 0) for (int n : chord2) midi.addEvent (juce::MidiMessage::noteOff (1, n), o);
        // hoher Ton zum Aliasing-Test
        if (int o = inBlock (8.5); o >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, 96, 0.6f), o);
        if (int o = inBlock (10.0); o >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, 96), o);

        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, buf, ch, 0, block);
    }

    bool bad = false;
    float peak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            const float v = out.getSample (ch, i);
            if (! std::isfinite (v)) bad = true;
            peak = juce::jmax (peak, std::abs (v));
        }

    std::printf ("peak=%.3f rmsL=%.4f rmsR=%.4f nan=%d\n", peak,
                 out.getRMSLevel (0, 0, out.getNumSamples()), out.getRMSLevel (1, 0, out.getNumSamples()), (int) bad);

    juce::File f (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("render.wav").getFullPathName());
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());

    // Editor erzeugen und alle Seiten als PNG speichern
    {
        juce::SharedResourcePointer<MossBackground::MossCache> cache;
        juce::Image a, b;
        for (int i = 0; i < 300 && ! cache->get (a, b); ++i) juce::Thread::sleep (50);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1200, 780);
    juce::PNGImageFormat png;
    for (int page = 0; page < 3; ++page)
    {
        if (auto* me = dynamic_cast<MoosEditor*> (ed.get())) me->showPage (page);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        auto pf = f.getSiblingFile ("ui" + juce::String (page) + ".png");
        pf.deleteFile();
        juce::FileOutputStream os (pf);
        png.writeImageToStream (img, os);
    }
    ed.reset();
    {
        auto moss = MossBackground::generateMoss();
        auto mf = f.getSiblingFile ("moss.png");
        mf.deleteFile();
        juce::FileOutputStream os (mf);
        png.writeImageToStream (moss, os);
    }

    // ---------- Funktionstests ----------
    auto renderChord = [&] (MoosProcessor& pr, double secs, std::vector<juce::MidiMessage> extra = {})
    {
        pr.prepareToPlay (sr, block);
        double sum = 0; long cnt = 0; bool finite = true;
        const int total = (int) (secs * sr);
        for (int pos = 0; pos + block <= total; pos += block)
        {
            juce::MidiBuffer midi;
            if (pos == 0) { for (int nn : { 48, 55, 60, 64 }) midi.addEvent (juce::MidiMessage::noteOn (1, nn, 0.9f), 0);
                            for (auto& m : extra) midi.addEvent (m, 1); }
            if (pos <= total / 2 && pos + block > total / 2) for (int nn : { 48, 55, 60, 64 }) midi.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < block; ++i)
            { float v = buf.getSample (ch, i); if (! std::isfinite (v)) finite = false; sum += v * v; ++cnt; }
        }
        return std::make_pair (std::sqrt (sum / cnt), finite);
    };

    int failures = bad ? 1 : 0;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        auto [rms, ok] = renderChord (proc, 3.0, { juce::MidiMessage::controllerEvent (1, 1, 100), juce::MidiMessage::channelPressureChange (1, 90) });
        std::printf ("preset %-24s rms=%.4f %s\n", proc.getProgramName (i).toRawUTF8(), rms, ok && rms > 0.002 ? "OK" : "FAIL");
        if (! ok || rms <= 0.002) ++failures;
    }

    // Wavetable-Dateien erzeugen und laden
    juce::File dir ("/tmp/moostest"); dir.createDirectory();
    auto writeWav = [&] (const juce::File& wf, const std::vector<float>& d)
    {
        wf.deleteFile();
        juce::WavAudioFormat w2;
        std::unique_ptr<juce::AudioFormatWriter> wr (w2.createWriterFor (new juce::FileOutputStream (wf), 48000, 1, 24, {}, 0));
        juce::AudioBuffer<float> b (1, (int) d.size());
        for (size_t i = 0; i < d.size(); ++i) b.setSample (0, (int) i, d[i]);
        wr->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    };
    std::vector<float> wt (2048 * 4), field (48000 * 3);
    for (int f = 0; f < 4; ++f) for (int i = 0; i < 2048; ++i)
        wt[(size_t) (f * 2048 + i)] = (float) std::sin (2 * juce::MathConstants<double>::pi * i / 2048 * (f + 1)) * 0.8f;
    juce::Random r (1);
    for (size_t i = 0; i < field.size(); ++i) field[i] = 0.3f * (r.nextFloat() - 0.5f) + 0.4f * (float) std::sin (i * 0.02);
    writeWav (dir.getChildFile ("table.wav"), wt);
    writeWav (dir.getChildFile ("field.wav"), field);

    proc.setCurrentProgram (0);
    auto e1 = proc.loadWavetable (0, dir.getChildFile ("table.wav"));
    auto e2 = proc.loadWavetable (1, dir.getChildFile ("field.wav"));
    std::printf ("wavetable load: '%s' frames=%d / '%s' frames=%d\n", e1.toRawUTF8(), proc.getBank (0)->getNumFrames(),
                 e2.toRawUTF8(), proc.getBank (1)->getNumFrames());
    { auto [rms, ok] = renderChord (proc, 2.0); std::printf ("custom tables rms=%.4f %s\n", rms, ok && rms > 0.002 ? "OK" : "FAIL"); if (! ok || rms <= 0.002) ++failures; }

    // Preset speichern + laden (inkl. Wavetable-Pfad)
    auto pf = dir.getChildFile ("Test.moospreset");
    proc.apvts.getParameter ("cutoff")->setValueNotifyingHost (0.123f);
    proc.saveUserPreset (pf);
    proc.setCurrentProgram (2);
    auto err = proc.loadUserPreset (pf);
    std::printf ("user preset: err='%s' cutoffNorm=%.3f bank0=%s name=%s\n", err.toRawUTF8(),
                 proc.apvts.getParameter ("cutoff")->getValue(), proc.getBank (0)->getName().toRawUTF8(), proc.getPresetName().toRawUTF8());

    // Mono + Glide + Sustain-Pedal
    proc.setCurrentProgram (7);
    { auto [rms, ok] = renderChord (proc, 2.0, { juce::MidiMessage::controllerEvent (1, 64, 127) });
      std::printf ("mono/sustain rms=%.4f %s\n", rms, ok && rms > 0.002 ? "OK" : "FAIL"); }

    // Shimmer-Stresstest: Vollanschlag 20 s
    for (float shim : { 0.0f, 0.5f, 1.0f })
    {
    proc.setCurrentProgram (1);
    proc.apvts.getParameter ("shimmer")->setValueNotifyingHost (shim);
    proc.apvts.getParameter ("reverbSize")->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter ("reverbMix")->setValueNotifyingHost (1.0f);
    {
        proc.prepareToPlay (sr, block);
        float firstHalf = 0, secondHalf = 0;
        const int total = (int) (20 * sr);
        for (int pos = 0; pos + block <= total; pos += block)
        {
            juce::MidiBuffer midi;
            if (pos == 0) for (int nn : { 48, 55, 60, 64 }) midi.addEvent (juce::MidiMessage::noteOn (1, nn, 0.9f), 0);
            if (pos <= (int) sr * 3 && pos + block > (int) sr * 3) for (int nn : { 48, 55, 60, 64 }) midi.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
            proc.processBlock (buf, midi);
            const float m = buf.getMagnitude (0, block);
            if (pos > total - (int) sr * 3) secondHalf = juce::jmax (secondHalf, m); else if (pos > sr * 3) firstHalf = juce::jmax (firstHalf, m);
        }
        std::printf ("shimmer %.1f stress: peak after release=%.3f, last 3 s=%.4f %s\n", shim, firstHalf, secondHalf, secondHalf < firstHalf * 0.5f ? "OK (klingt aus)" : "WARN");
        if (shim == 1.0f && secondHalf >= firstHalf * 0.5f) ++failures;
    }
    }

    // Hörprobe: mehrere Presets hintereinander
    {
        const int presets[] = { 1, 3, 2, 9, 4, 6 };
        const int per = (int) (7 * sr);
        juce::AudioBuffer<float> demo (2, per * 6);
        demo.clear();
        int at = 0;
        for (int pi : presets)
        {
            proc.setCurrentProgram (pi);
            proc.prepareToPlay (sr, block);
            const std::vector<int> chord = pi == 3 ? std::vector<int> { 38, 45, 50 } : std::vector<int> { 50, 57, 60, 64, 69 };
            for (int pos = 0; pos + block <= per; pos += block)
            {
                juce::MidiBuffer midi;
                if (pos == 0) for (int nn : chord) midi.addEvent (juce::MidiMessage::noteOn (1, nn, 0.85f), 0);
                if (pos == 0 && pi == 9) midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 90), 0);
                const int offAt = (int) (4.5 * sr);
                if (pos <= offAt && pos + block > offAt) for (int nn : chord) midi.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
                proc.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, at + pos, buf, ch, 0, block);
            }
            // kurzer Fade am Ende jedes Abschnitts
            for (int ch = 0; ch < 2; ++ch) demo.applyGainRamp (ch, at + per - (int) (0.4 * sr), (int) (0.4 * sr), 1.0f, 0.0f);
            at += per;
        }
        auto df = f.getSiblingFile ("demo.wav");
        df.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> dw (wav.createWriterFor (new juce::FileOutputStream (df), sr, 2, 24, {}, 0));
        dw->writeFromAudioSampleBuffer (demo, 0, demo.getNumSamples());
    }

    std::printf ("failures=%d\n", failures);
    return failures;
}

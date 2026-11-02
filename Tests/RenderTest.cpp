// Offline-Test: spielt einen Akkord durch MOOS und schreibt render.wav
#include "../Source/PluginProcessor.h"
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

    // Editor einmal erzeugen, um Abstürze in der UI zu erkennen
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (980, 600);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds());
    juce::PNGImageFormat png;
    juce::FileOutputStream os (f.getSiblingFile ("ui.png"));
    if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
    ed.reset();
    return bad ? 1 : 0;
}

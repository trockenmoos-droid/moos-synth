#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace tmk
{
    // juce::ADSR mit sicherem Parameterwechsel.
    // Hintergrund: juce::ADSR::setParameters() berechnet die Release-Geschwindigkeit aus dem Sustain-Pegel neu –
    // auch mitten im Ausklingen. Ist Sustain klein (oder 0), klingt eine losgelassene Note dann extrem lange
    // (oder nie) aus. Hier werden Parameter nur bei echter Änderung gesetzt, und eine bereits losgelassene
    // Hüllkurve klingt danach vom aktuellen Pegel aus mit der richtigen Release-Zeit aus.
    class Adsr : public juce::ADSR
    {
    public:
        void setParameters (const Parameters& p) noexcept
        {
            if (p.attack == last.attack && p.decay == last.decay && p.sustain == last.sustain && p.release == last.release)
                return;
            last = p;
            juce::ADSR::setParameters (p);
            if (released && isActive()) juce::ADSR::noteOff();
        }
        void setSampleRate (double sr) noexcept { juce::ADSR::setSampleRate (sr); if (released && isActive()) juce::ADSR::noteOff(); }
        void noteOn() noexcept  { released = false; juce::ADSR::noteOn(); }
        void noteOff() noexcept { released = true;  juce::ADSR::noteOff(); }
        void reset() noexcept   { released = false; juce::ADSR::reset(); }

    private:
        bool released = false;
        Parameters last { -1.0f, -1.0f, -1.0f, -1.0f };
    };
}

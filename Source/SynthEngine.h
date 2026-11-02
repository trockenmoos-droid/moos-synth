#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Wavetable.h"
#include "Params.h"

// Eigene Stimmenverwaltung: Poly (16 Stimmen) oder Mono/Legato mit Glide,
// Sustain-Pedal, Aftertouch, Mod-Wheel, 2 globale LFOs, Mod-Matrix, Analog-Drift.
class SynthEngine
{
public:
    static constexpr int maxVoices  = 16;
    static constexpr int maxUnison  = 7;
    static constexpr int subBlock   = 16;

    void init (juce::AudioProcessorValueTreeState& s)
    {
        auto get = [&s] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
        posA = get ("posA"); octA = get ("octA"); levelA = get ("levelA");
        posB = get ("posB"); octB = get ("octB"); fineB = get ("fineB"); levelB = get ("levelB");
        uniVoices = get ("uniVoices"); uniDetune = get ("uniDetune"); uniWidth = get ("uniWidth");
        cutoff = get ("cutoff"); reso = get ("reso"); envAmt = get ("envAmt");
        fA = get ("fA"); fD = get ("fD"); fS = get ("fS"); fR = get ("fR");
        aA = get ("aA"); aD = get ("aD"); aS = get ("aS"); aR = get ("aR");
        lfoRate = get ("lfoRate"); lfoShape = get ("lfoShape"); lfoPos = get ("lfoPos"); lfoCut = get ("lfoCut");
        lfo2Rate = get ("lfo2Rate"); lfo2Shape = get ("lfo2Shape");
        drift = get ("drift"); glide = get ("glide"); mono = get ("mono"); bendRange = get ("bendRange");
        for (int i = 0; i < MoosParams::numModSlots; ++i)
        {
            const auto n = juce::String (i + 1);
            modSrc[i] = get ("mod" + n + "Src");
            modDst[i] = get ("mod" + n + "Dst");
            modAmt[i] = get ("mod" + n + "Amt");
        }
    }

    void setBank (int osc, const WavetableBank* b) noexcept { banks[osc].store (b); }
    const WavetableBank* getBank (int osc) const noexcept   { return banks[osc].load(); }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) { v.amp.setSampleRate (sr); v.flt.setSampleRate (sr); v.active = false; }
        noteStack.clear();
        sustainPedal = false;
    }

    void allNotesOff()
    {
        for (auto& v : voices) { v.held = false; v.sustained = false; v.amp.noteOff(); v.flt.noteOff(); }
        noteStack.clear();
    }

    // Live-Werte für die Oberfläche
    float getLfoValue (int i) const noexcept { return i == 0 ? lfoVal[0] : lfoVal[1]; }
    float getModWheel() const noexcept       { return modWheel; }

    void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, int numSamples)
    {
        readBlockParams();

        int pos = 0;
        for (const auto meta : midi)
        {
            const int t = juce::jlimit (0, numSamples, meta.samplePosition);
            if (t > pos) { render (out, pos, t - pos); pos = t; }
            handleMidi (meta.getMessage());
        }
        if (pos < numSamples)
            render (out, pos, numSamples - pos);
    }

private:
    using AF = std::atomic<float>*;
    AF posA{}, octA{}, levelA{}, posB{}, octB{}, fineB{}, levelB{}, uniVoices{}, uniDetune{}, uniWidth{};
    AF cutoff{}, reso{}, envAmt{}, fA{}, fD{}, fS{}, fR{}, aA{}, aD{}, aS{}, aR{};
    AF lfoRate{}, lfoShape{}, lfoPos{}, lfoCut{}, lfo2Rate{}, lfo2Shape{};
    AF drift{}, glide{}, mono{}, bendRange{};
    AF modSrc[MoosParams::numModSlots]{}, modDst[MoosParams::numModSlots]{}, modAmt[MoosParams::numModSlots]{};

    std::atomic<const WavetableBank*> banks[2] { nullptr, nullptr };

    struct Voice
    {
        bool  active = false, held = false, sustained = false;
        int   note = 60;
        float velocity = 1.0f, polyAT = 0.0f, randVal = 0.0f;
        juce::uint64 age = 0;
        double pitch = 60.0, targetPitch = 60.0;
        double phases[2][maxUnison] {};
        float  driftCur[2][maxUnison] {}, driftTgt[2][maxUnison] {};
        float  cutDrift = 0.0f, cutDriftTgt = 0.0f;
        float  lastPos[2] { 0.0f, 0.0f }, lastGain = 0.0f;
        float  ic1L = 0, ic2L = 0, ic1R = 0, ic2R = 0;
        juce::ADSR amp, flt;
    };

    Voice voices[maxVoices];
    std::vector<int> noteStack;
    juce::Random rng;
    juce::uint64 ageCounter = 0;
    double sr = 44100.0;
    double lastPitch = 60.0;
    bool sustainPedal = false, prevMono = false;
    float modWheel = 0.0f, channelAT = 0.0f, bend = 0.0f;

    double lfoPhase[2] { 0.0, 0.0 };
    float  lfoVal[2] { 0.0f, 0.0f }, lfoSH[2] { 0.0f, 0.0f };

    // Block-Parameter
    struct BP
    {
        float posA, posB, levelA, levelB, uniDetune, uniWidth, cutoff, k, envAmt, lfoPos, lfoCut, drift, glide;
        int octA, octB, nUni, lfoShape[2];
        float fineB, bendRange;
        double lfoInc[2];
        int src[MoosParams::numModSlots], dst[MoosParams::numModSlots];
        float amt[MoosParams::numModSlots];
        bool mono;
    } bp {};

    void readBlockParams()
    {
        bp.posA = posA->load(); bp.posB = posB->load(); bp.levelA = levelA->load(); bp.levelB = levelB->load();
        bp.octA = (int) std::round (octA->load()); bp.octB = (int) std::round (octB->load());
        bp.fineB = fineB->load();
        bp.nUni = juce::jlimit (1, maxUnison, (int) std::round (uniVoices->load()));
        bp.uniDetune = uniDetune->load(); bp.uniWidth = uniWidth->load();
        bp.cutoff = cutoff->load(); bp.k = 2.0f - 1.96f * reso->load(); bp.envAmt = envAmt->load();
        bp.lfoPos = lfoPos->load(); bp.lfoCut = lfoCut->load();
        bp.lfoInc[0] = lfoRate->load() / sr;  bp.lfoShape[0] = (int) lfoShape->load();
        bp.lfoInc[1] = lfo2Rate->load() / sr; bp.lfoShape[1] = (int) lfo2Shape->load();
        bp.drift = drift->load(); bp.glide = glide->load(); bp.bendRange = bendRange->load();
        bp.mono = mono->load() > 0.5f;
        if (bp.mono != prevMono) { prevMono = bp.mono; allNotesOff(); }
        for (int i = 0; i < MoosParams::numModSlots; ++i)
        {
            bp.src[i] = (int) modSrc[i]->load(); bp.dst[i] = (int) modDst[i]->load(); bp.amt[i] = modAmt[i]->load();
        }

        const juce::ADSR::Parameters ap { aA->load(), aD->load(), aS->load(), aR->load() };
        const juce::ADSR::Parameters fp { fA->load(), fD->load(), fS->load(), fR->load() };
        for (auto& v : voices) { v.amp.setParameters (ap); v.flt.setParameters (fp); }
    }

    //==========================================================================
    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())                   noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())             noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())          bend = (float) ((m.getPitchWheelValue() - 8192) / 8192.0);
        else if (m.isChannelPressure())     channelAT = (float) m.getChannelPressureValue() / 127.0f;
        else if (m.isAftertouch())
        {
            for (auto& v : voices)
                if (v.active && v.note == m.getNoteNumber())
                    v.polyAT = (float) m.getAfterTouchValue() / 127.0f;
        }
        else if (m.isSustainPedalOn())      sustainPedal = true;
        else if (m.isSustainPedalOff())
        {
            sustainPedal = false;
            for (auto& v : voices)
                if (v.sustained && ! v.held) { v.sustained = false; release (v); }
        }
        else if (m.isController() && m.getControllerNumber() == 1)
            modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            allNotesOff();
    }

    void release (Voice& v) { v.amp.noteOff(); v.flt.noteOff(); }

    void startVoice (Voice& v, int note, float vel, bool fromGlide)
    {
        const bool wasSilent = ! v.active;
        v.active = true; v.held = true; v.sustained = false;
        v.note = note; v.velocity = vel; v.polyAT = 0.0f;
        v.age = ++ageCounter;
        v.randVal = rng.nextFloat() * 2.0f - 1.0f;
        v.targetPitch = note;
        v.pitch = (fromGlide && bp.glide > 0.001f) ? lastPitch : (double) note;

        if (wasSilent)
        {
            for (int o = 0; o < 2; ++o)
                for (int u = 0; u < maxUnison; ++u)
                {
                    v.phases[o][u] = rng.nextDouble();      // zufällige Startphasen: breiter Klang
                    v.driftCur[o][u] = v.driftTgt[o][u] = rng.nextFloat() * 2.0f - 1.0f;
                }
            v.ic1L = v.ic2L = v.ic1R = v.ic2R = 0.0f;
            v.lastPos[0] = bp.posA; v.lastPos[1] = bp.posB;
            v.lastGain = 0.0f;
            v.amp.reset(); v.flt.reset();
        }
        v.amp.noteOn();
        v.flt.noteOn();
        lastPitch = note;
    }

    void noteOn (int note, float vel)
    {
        if (bp.mono)
        {
            noteStack.erase (std::remove (noteStack.begin(), noteStack.end(), note), noteStack.end());
            noteStack.push_back (note);
            auto& v = voices[0];
            for (int i = 1; i < maxVoices; ++i) if (voices[i].active) release (voices[i]);

            if (v.active && (v.held || v.sustained) && v.amp.isActive())
            {
                // Legato: Tonhöhe gleitet, Hüllkurven laufen weiter
                v.targetPitch = note; v.note = note; v.held = true;
                if (bp.glide <= 0.001f) v.pitch = note;
                lastPitch = note;
            }
            else
                startVoice (v, note, vel, true);
            return;
        }

        // Poly: gleiche Note wiederverwenden, sonst freie Stimme, sonst älteste stehlen
        Voice* target = nullptr;
        for (auto& v : voices) if (v.active && v.note == note) { target = &v; break; }
        if (target == nullptr) for (auto& v : voices) if (! v.active) { target = &v; break; }
        if (target == nullptr)
        {
            juce::uint64 best = std::numeric_limits<juce::uint64>::max();
            for (auto& v : voices)    // bevorzugt losgelassene Stimmen
                if (! v.held && ! v.sustained && v.age < best) { best = v.age; target = &v; }
            if (target == nullptr)
                for (auto& v : voices) if (v.age < best) { best = v.age; target = &v; }
        }
        startVoice (*target, note, vel, true);
    }

    void noteOff (int note)
    {
        if (bp.mono)
        {
            noteStack.erase (std::remove (noteStack.begin(), noteStack.end(), note), noteStack.end());
            auto& v = voices[0];
            if (! v.active) return;
            if (! noteStack.empty())
            {
                if (v.note == note) { v.note = noteStack.back(); v.targetPitch = v.note; if (bp.glide <= 0.001f) v.pitch = v.note; lastPitch = v.note; }
            }
            else if (sustainPedal) { v.held = false; v.sustained = true; }
            else { v.held = false; release (v); }
            return;
        }

        for (auto& v : voices)
            if (v.active && v.held && v.note == note)
            {
                v.held = false;
                if (sustainPedal) v.sustained = true;
                else release (v);
            }
    }

    //==========================================================================
    static float lfoShapeValue (int shape, double ph, float sh) noexcept
    {
        switch (shape)
        {
            case 1:  return 4.0f * std::abs ((float) ph - 0.5f) - 1.0f;
            case 2:  return 2.0f * (float) ph - 1.0f;
            case 3:  return ph < 0.5 ? 1.0f : -1.0f;
            case 4:  return sh;
            default: return (float) std::sin (ph * juce::MathConstants<double>::twoPi);
        }
    }

    void render (juce::AudioBuffer<float>& out, int start, int num)
    {
        const WavetableBank* bank[2] = { banks[0].load(), banks[1].load() };
        if (bank[0] == nullptr || bank[1] == nullptr) return;

        for (int s0 = 0; s0 < num; s0 += subBlock)
        {
            const int n = juce::jmin (subBlock, num - s0);

            for (int l = 0; l < 2; ++l)
            {
                lfoPhase[l] += bp.lfoInc[l] * n;
                if (lfoPhase[l] >= 1.0) { lfoPhase[l] -= std::floor (lfoPhase[l]); lfoSH[l] = rng.nextFloat() * 2.0f - 1.0f; }
                lfoVal[l] = lfoShapeValue (bp.lfoShape[l], lfoPhase[l], lfoSH[l]);
            }

            for (auto& v : voices)
                if (v.active)
                    renderVoice (v, bank, out, start + s0, n);
        }
    }

    void renderVoice (Voice& v, const WavetableBank* const* bank, juce::AudioBuffer<float>& out, int start, int n)
    {
        using namespace MoosParams;

        // --- Glide ---
        if (bp.glide > 0.001f)
        {
            const double coef = std::exp (-(double) n / (bp.glide / 3.0 * sr));
            v.pitch = v.targetPitch + (v.pitch - v.targetPitch) * coef;
        }
        else v.pitch = v.targetPitch;

        // --- Mod-Matrix ---
        const float fenv = v.flt.getNextSample();
        for (int i = 1; i < n; ++i) v.flt.getNextSample();

        float src[9];
        src[srcOff] = 0.0f; src[srcLfo1] = lfoVal[0]; src[srcLfo2] = lfoVal[1];
        src[srcModWheel] = modWheel; src[srcAftertouch] = juce::jmax (channelAT, v.polyAT);
        src[srcVelocity] = v.velocity; src[srcFiltEnv] = fenv; src[srcRandom] = v.randVal;
        src[srcKey] = juce::jlimit (-1.0f, 1.0f, (float) (v.note - 60) / 36.0f);

        float d[numDests] {};
        for (int i = 0; i < numModSlots; ++i)
            if (bp.src[i] != srcOff)
                d[juce::jlimit (0, numDests - 1, bp.dst[i])] += bp.amt[i] * src[juce::jlimit (0, 8, bp.src[i])];

        // --- Drift (langsamer Zufallsweg) ---
        const float driftSmooth = 1.0f - std::exp (-(float) n / (0.8f * (float) sr));
        const float changeProb  = (float) n / (1.2f * (float) sr);
        for (int o = 0; o < 2; ++o)
            for (int u = 0; u < bp.nUni; ++u)
            {
                if (rng.nextFloat() < changeProb) v.driftTgt[o][u] = rng.nextFloat() * 2.0f - 1.0f;
                v.driftCur[o][u] += (v.driftTgt[o][u] - v.driftCur[o][u]) * driftSmooth;
            }
        if (rng.nextFloat() < changeProb) v.cutDriftTgt = rng.nextFloat() * 2.0f - 1.0f;
        v.cutDrift += (v.cutDriftTgt - v.cutDrift) * driftSmooth;

        // --- Oszillatoren ---
        const float lvl[2] = { juce::jlimit (0.0f, 1.0f, bp.levelA + d[dstLevelA]),
                               juce::jlimit (0.0f, 1.0f, bp.levelB + d[dstLevelB]) };
        // Pitch-Modulation quadratisch skaliert: feines Vibrato bei kleinen Werten, ±12 HT bei ±1
        const double basePitch = v.pitch + bend * bp.bendRange + 12.0 * d[dstPitch] * std::abs (d[dstPitch]);
        const double oscPitch[2] = { basePitch + 12.0 * bp.octA,
                                     basePitch + 12.0 * bp.octB + (bp.fineB + 100.0f * d[dstFineB]) / 100.0 };
        const float spread  = juce::jlimit (0.0f, 100.0f, bp.uniDetune + 60.0f * d[dstUniDetune]);
        const float driftCt = bp.drift * 12.0f;
        const int   nUni    = bp.nUni;
        const float uniGain = 1.0f / std::sqrt ((float) nUni);

        double inc[2][maxUnison];
        float  panL[maxUnison], panR[maxUnison];
        int    level[2];
        for (int o = 0; o < 2; ++o)
        {
            const double hz = 440.0 * std::pow (2.0, (oscPitch[o] - 69.0) / 12.0);
            level[o] = WavetableBank::levelForFrequency (hz * std::pow (2.0, (spread + driftCt) / 1200.0), sr);
            for (int u = 0; u < nUni; ++u)
            {
                const float t = (nUni == 1) ? 0.0f : (2.0f * (float) u / (float) (nUni - 1) - 1.0f);
                const double cents = t * spread + v.driftCur[o][u] * driftCt;
                inc[o][u] = juce::jmin (0.49, hz * std::pow (2.0, cents / 1200.0) / sr);
            }
        }
        for (int u = 0; u < nUni; ++u)
        {
            const float t = (nUni == 1) ? 0.0f : (2.0f * (float) u / (float) (nUni - 1) - 1.0f);
            const float angle = (t * bp.uniWidth + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            panL[u] = std::cos (angle) * uniGain;
            panR[u] = std::sin (angle) * uniGain;
        }

        const float targetPos[2] = {
            juce::jlimit (0.0f, 1.0f, bp.posA + 0.5f * bp.lfoPos * lfoVal[0] + d[dstPosA]),
            juce::jlimit (0.0f, 1.0f, bp.posB + 0.5f * bp.lfoPos * lfoVal[0] + d[dstPosB])
        };

        // --- Filter ---
        const float cutHz = juce::jlimit (20.0f, (float) (0.45 * sr),
            bp.cutoff * std::pow (2.0f, bp.envAmt * 6.0f * fenv + bp.lfoCut * 3.0f * lfoVal[0]
                                        + 5.0f * d[dstCutoff] + bp.drift * 0.3f * v.cutDrift));
        const float k  = juce::jlimit (0.04f, 2.0f, bp.k - 1.96f * d[dstReso]);
        const float g  = (float) std::tan (juce::MathConstants<double>::pi * cutHz / sr);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float velGain    = 0.3f + 0.7f * v.velocity;
        const float targetGain = juce::jlimit (0.0f, 2.0f, 1.0f + d[dstVolume]) * velGain * 0.35f;

        auto* L = out.getWritePointer (0);
        auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            const float frac = (float) (i + 1) / (float) n;
            float sl = 0.0f, sr_ = 0.0f;

            for (int o = 0; o < 2; ++o)
            {
                if (lvl[o] <= 0.0001f) continue;
                const int   nf  = bank[o]->getNumFrames();
                const float pos = v.lastPos[o] + (targetPos[o] - v.lastPos[o]) * frac;
                const float fp  = pos * (float) (nf - 1);
                const int   f0  = juce::jlimit (0, nf - 1, (int) fp);
                const int   f1  = juce::jmin (f0 + 1, nf - 1);
                const float ff  = fp - (float) f0;
                const float* t0 = bank[o]->get (level[o], f0);
                const float* t1 = bank[o]->get (level[o], f1);

                for (int u = 0; u < nUni; ++u)
                {
                    double& ph = v.phases[o][u];
                    const double idx = ph * WavetableBank::tableSize;
                    const int    ii  = (int) idx;
                    const float  fr  = (float) (idx - ii);
                    const float  a   = t0[ii] + fr * (t0[ii + 1] - t0[ii]);
                    const float  b   = t1[ii] + fr * (t1[ii + 1] - t1[ii]);
                    const float  s   = (a + ff * (b - a)) * lvl[o];
                    sl += s * panL[u]; sr_ += s * panR[u];
                    ph += inc[o][u];
                    if (ph >= 1.0) ph -= 1.0;
                }
            }

            const float outL = svf (sl,  v.ic1L, v.ic2L, a1, a2, a3);
            const float outR = svf (sr_, v.ic1R, v.ic2R, a1, a2, a3);
            const float gain = (v.lastGain + (targetGain - v.lastGain) * frac) * v.amp.getNextSample();

            L[start + i] += outL * gain;
            if (R != nullptr) R[start + i] += outR * gain;
        }

        v.lastPos[0] = targetPos[0]; v.lastPos[1] = targetPos[1];
        v.lastGain = targetGain;

        if (! v.amp.isActive())
        {
            v.active = false; v.held = false; v.sustained = false;
        }
    }

    static inline float svf (float x, float& ic1, float& ic2, float a1, float a2, float a3) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

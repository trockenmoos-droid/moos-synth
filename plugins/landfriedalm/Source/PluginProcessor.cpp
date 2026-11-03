#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace alm;

// Formanten (Hz) für a, o, u, e, i
static const float vowelF[5][3] = { { 800, 1150, 2800 }, { 450, 800, 2830 }, { 325, 700, 2700 }, { 450, 2000, 2700 }, { 300, 2300, 3000 } };
// Kuhglocke: Teiltöne (unharmonisch) und relative Abklingzeit
static const float bellR[5] = { 1.0f, 1.52f, 2.23f, 2.98f, 3.81f };
static const float bellD[5] = { 1.0f, 0.7f, 0.45f, 0.3f, 0.2f };
static const float bellA[5] = { 1.0f, 0.8f, 0.6f, 0.45f, 0.3f };

void AlmVoice::setup (double sampleRate, const AlmParams* p, juce::uint32 seed)
{
    sr = sampleRate; params = p; rng.setSeed ((juce::int64) seed);
    for (auto& s : str) s.prepare (sr);
    scratch.assign ((size_t) (sr / 20.0) + 8, 0.0f);
    env.setSampleRate (sr);
}

void AlmVoice::start (bool)
{
    const auto& p = *params;
    inst = p.instrument;
    const double f = hz (note);
    const float vel = 0.3f + 0.7f * velocity;
    releaseGain = 1.0f; releasing = false; silentCount = 0;

    // Holzkörper der Zither / des Hackbretts
    const double bodyF[3] = { 190.0, 430.0, 960.0 };
    for (int b = 0; b < 3; ++b) { body[b].reset(); body[b].set (bodyF[b], sr, 4.0f); }

    if (inst == zither || inst == hackbrett)
    {
        const int strings = inst == hackbrett ? 2 : 1;
        const float cents = inst == hackbrett ? 3.0f + p.schwebung * 14.0f : p.schwebung * 3.0f;
        for (int s = 0; s < strings; ++s)
        {
            const double fs = f * std::pow (2.0, (s == 0 ? -cents : cents) * 0.5 / 1200.0);
            str[s].tune (fs, sr, p.abklingen * (inst == hackbrett ? 0.8f : 1.0f), p.hell);
            // Anregung: Zither = gezupft (Rauschen, Zupfposition), Hackbrett = Hammer (kurzer Puls)
            const int len = juce::jlimit (2, (int) scratch.size(), (int) str[s].delay);
            float* ex = scratch.data();
            std::fill (ex, ex + len, 0.0f);
            if (inst == zither)
            {
                const int pluckPos = juce::jmax (1, (int) (len * (0.08f + 0.35f * (1.0f - p.anschlag))));
                float lp = 0.0f;
                for (int i = 0; i < len; ++i)
                {
                    const float x = rng.nextFloat() * 2.0f - 1.0f;
                    lp = x + (0.1f + 0.75f * (1.0f - p.anschlag)) * (lp - x);
                    ex[i] = lp;
                }
                for (int i = len - 1; i >= pluckPos; --i) ex[i] -= ex[i - pluckPos];   // Kammfilter = Zupfposition
            }
            else
            {
                const int hw = juce::jmax (2, (int) (len * (0.25f - 0.2f * p.anschlag)));
                for (int i = 0; i < hw && i < len; ++i)
                    ex[i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / hw);
            }
            for (int i = 0; i < len; ++i) ex[i] *= vel * 0.8f;
            str[s].excite (ex, len);
        }
    }
    else if (inst == kuhglocke)
    {
        for (int k = 0; k < 5; ++k)
        {
            const float a = bellA[k] * std::pow ((float) (k + 1), (p.hell - 0.5f) * 0.9f) * vel;
            mAmp[k] = a;
            mCoef[k] = (float) std::exp (-6.9 / (juce::jmax (0.05f, p.abklingen * 0.6f * bellD[k]) * sr));
            mPhase[k][0] = mPhase[k][1] = 0.0;
        }
        clank = 0.7f * vel * (0.4f + 0.6f * p.anschlag);
        clankFilter.reset(); clankFilter.set (f * 2.6, sr, 2.0f);
    }
    else
    {
        // Jodler: beginnt in der Bruststimme, kippt nach kurzer Zeit in die Kopfstimme (Registersprung)
        pitchOffset = -p.jodelsprung * 7.0f;
        flipTimer = 0.07f + 0.1f * (1.0f - p.anschlag);
        ageSec = 0.0f; headMix = 0.0f; vibPhase = 0.0;
        for (auto& fm : formant) fm.reset();
        aspiration = 0.25f;
    }

    env.setParameters ({ inst == jodler ? 0.03f : 0.001f, 0.1f, 1.0f,
                         inst == jodler ? 0.15f + 0.1f * p.abklingen : 0.08f });
    env.noteOn();
}

void AlmVoice::release()
{
    env.noteOff();
    releasing = true;
    // gezupfte/geschlagene Instrumente: Loslassen dämpft (Saite mit Hand abgefangen), aber weich
    releaseCoef = (float) std::exp (-6.9 / (0.35 * sr));
}

void AlmVoice::render (float* L, float* R, int n, const tmk::PlayContext& ctx)
{
    const auto& p = *params;
    const double f = hz (note + ctx.bend);
    const float pan = juce::jlimit (-0.7f, 0.7f, (note - 62) / 30.0f);
    const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
    float peak = 0.0f;

    if (inst == jodler)
    {
        // Vokal wandert mit dem Mod Wheel, Aftertouch = mehr Vibrato
        const float vow = juce::jlimit (0.0f, 1.0f, p.vokal + 0.5f * ctx.modWheel) * 4.0f;
        const int v0 = juce::jmin (3, (int) vow);
        const float vf = vow - (float) v0;
        for (int i = 0; i < n; ++i)
        {
            ageSec += (float) (1.0 / sr);
            if (ageSec > flipTimer) pitchOffset *= 0.996f;                     // Sprung nach oben (schnelles Gleiten)
            headMix = juce::jlimit (0.0f, 1.0f, (ageSec - flipTimer) * 12.0f) * p.jodelsprung;
            const float vibDepth = (0.08f + 0.35f * (p.schwebung + ctx.aftertouch)) * juce::jlimit (0.0f, 1.0f, (ageSec - 0.25f) * 2.0f);
            vibPhase += 5.4 / sr; if (vibPhase >= 1.0) vibPhase -= 1.0;
            const double fNow = f * std::pow (2.0, (pitchOffset + vibDepth * tmk::dsp::sine (vibPhase)) / 12.0);
            const double inc = fNow / sr;
            gPhase += inc; if (gPhase >= 1.0) gPhase -= 1.0;
            // Stimmquelle: Brust = Sägezahn (obertonreich), Kopf = weicher (mehr Grundton)
            const float saw = tmk::dsp::sawBlep (gPhase, inc);
            const float soft = tmk::dsp::sine (gPhase);
            float src = saw * (1.0f - 0.7f * headMix) + soft * 0.9f * headMix;
            src += (rng.nextFloat() * 2.0f - 1.0f) * aspiration * 0.15f;
            aspiration *= 0.9995f;

            if ((i & 15) == 0)
            {
                // Kopfstimme: Formanten etwas höher, Vokal Richtung "u/i"
                const float shift = 1.0f + 0.18f * headMix;
                for (int k = 0; k < 3; ++k)
                {
                    const float fk = (vowelF[v0][k] * (1.0f - vf) + vowelF[v0 + 1][k] * vf) * shift;
                    formant[k].set (fk, sr, k == 0 ? 6.0f : 9.0f);
                }
            }
            const float hellGain = 0.4f + 0.8f * p.hell;
            float y = formant[0].bandpass (src) * 1.0f + formant[1].bandpass (src) * 0.6f * hellGain + formant[2].bandpass (src) * 0.3f * hellGain;
            y *= env.getNextSample() * (0.35f + 0.65f * velocity) * 0.5f;
            L[i] += y * gl; R[i] += y * gr;
            peak = juce::jmax (peak, std::abs (y));
        }
        if (! env.isActive()) active = false;
        return;
    }

    if (inst == kuhglocke)
    {
        const float beat = 0.5f + p.schwebung * 4.0f;
        double inc[5][2];
        for (int k = 0; k < 5; ++k)
        {
            const double fk = juce::jmin (f * bellR[k], 0.45 * sr);
            inc[k][0] = (fk - beat * 0.5) / sr; inc[k][1] = (fk + beat * 0.5) / sr;
        }
        for (int i = 0; i < n; ++i)
        {
            float y = 0.0f;
            for (int k = 0; k < 5; ++k)
            {
                y += mAmp[k] * (tmk::dsp::sine (mPhase[k][0]) + 0.7f * tmk::dsp::sine (mPhase[k][1]));
                mPhase[k][0] += inc[k][0]; if (mPhase[k][0] >= 1.0) mPhase[k][0] -= 1.0;
                mPhase[k][1] += inc[k][1]; if (mPhase[k][1] >= 1.0) mPhase[k][1] -= 1.0;
                mAmp[k] *= mCoef[k];
            }
            if (clank > 1.0e-4f) { y += clankFilter.bandpass ((rng.nextFloat() * 2.0f - 1.0f) * clank) * 2.0f; clank *= 0.997f; }
            // Holz/Glockenkörper färbt mit
            float bsum = 0.0f;
            for (auto& b : body) bsum += b.bandpass (y);
            y = y * 0.12f + bsum * 0.03f * p.koerper;
            if (releasing) releaseGain *= releaseCoef * 1.0f + (1.0f - releaseCoef) * 0.9f;   // Kuhglocke klingt fast frei aus
            y *= releaseGain;
            L[i] += y * gl; R[i] += y * gr;
            peak = juce::jmax (peak, std::abs (y));
        }
    }
    else
    {
        const int strings = inst == hackbrett ? 2 : 1;
        for (int i = 0; i < n; ++i)
        {
            float y = 0.0f;
            for (int s = 0; s < strings; ++s) y += str[s].process();
            float bsum = 0.0f;
            for (auto& b : body) bsum += b.bandpass (y);
            y = (y * (1.0f - 0.5f * p.koerper) + bsum * 0.6f * p.koerper) * (inst == zither ? 0.9f : 0.45f);
            if (releasing) releaseGain *= releaseCoef;
            y *= releaseGain;
            L[i] += y * gl; R[i] += y * gr;
            peak = juce::jmax (peak, std::abs (y));
        }
    }

    // Stimme frei, wenn lange genug still
    if (peak < 1.0e-4f) { if (++silentCount > 8) active = false; } else silentCount = 0;
    if (releasing && releaseGain < 1.0e-4f) active = false;
}

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PC = juce::AudioParameterChoice; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<PC> (ID { "instrument", 1 }, "Instrument", instruments, 0));
    p.push_back (std::make_unique<P>  (ID { "hell", 1 },       "Helligkeit", 0.0f, 1.0f, 0.6f));
    p.push_back (std::make_unique<P>  (ID { "abklingen", 1 },  "Abklingen (s)", tmk::skewed (0.1f, 12.0f, 2.0f), 3.0f));
    p.push_back (std::make_unique<P>  (ID { "koerper", 1 },    u8 ("K\xc3\xb6rper"), 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "schwebung", 1 },  "Schwebung", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "anschlag", 1 },   "Anschlag", 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "vokal", 1 },      "Vokal a-o-u-e-i", 0.0f, 1.0f, 0.15f));
    p.push_back (std::make_unique<P>  (ID { "jodelsprung", 1 },"Jodelsprung", 0.0f, 1.0f, 0.6f));
    p.push_back (std::make_unique<P>  (ID { "echo", 1 },       "Echo vom Berg", 0.0f, 1.0f, 0.25f));
    p.push_back (std::make_unique<P>  (ID { "echoZeit", 1 },   "Echozeit (ms)", tmk::skewed (80.0f, 1500.0f, 400.0f), 520.0f));
    p.push_back (std::make_unique<P>  (ID { "hall", 1 },       "Hall", 0.0f, 1.0f, 0.2f));
    p.push_back (std::make_unique<P>  (ID { "volume", 1 },     u8 ("Lautst\xc3\xa4rke (dB)"), -48.0f, 6.0f, -6.0f));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Zither am Abend", {} },
        { "Helle Konzertzither",  { { "hell", 0.85f }, { "anschlag", 0.8f }, { "koerper", 0.35f }, { "abklingen", 4.0f } } },
        { "Hackbrett",            { { "instrument", 1 }, { "hell", 0.7f }, { "schwebung", 0.35f }, { "abklingen", 3.5f }, { "koerper", 0.6f } } },
        { "Hackbrett gedeckt",    { { "instrument", 1 }, { "hell", 0.3f }, { "anschlag", 0.2f }, { "abklingen", 2.5f }, { "hall", 0.35f } } },
        { "Kuhglocken-Herde",     { { "instrument", 2 }, { "abklingen", 2.0f }, { "schwebung", 0.6f }, { "echo", 0.45f }, { "echoZeit", 700.0f } } },
        { "Kleine Schelle",       { { "instrument", 2 }, { "hell", 0.85f }, { "abklingen", 0.8f }, { "anschlag", 0.8f } } },
        { "Jodler",               { { "instrument", 3 }, { "echo", 0.4f }, { "echoZeit", 650.0f } } },
        { "Juchzer",              { { "instrument", 3 }, { "jodelsprung", 1.0f }, { "vokal", 0.75f }, { "hell", 0.8f }, { "anschlag", 0.8f },
                                    { "echo", 0.5f }, { "echoZeit", 800.0f } } },
        { "Sanfter Almsang",      { { "instrument", 3 }, { "jodelsprung", 0.15f }, { "vokal", 0.3f }, { "hell", 0.35f }, { "abklingen", 6.0f },
                                    { "hall", 0.45f }, { "schwebung", 0.5f } } },
    };
}

AlmProcessor::AlmProcessor()
    : tmk::Processor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "LANDFRIEDALM", factoryPresets())
{
    auto get = [this] (const char* i) { return apvts.getRawParameterValue (i); };
    instrument = get ("instrument"); hell = get ("hell"); abklingen = get ("abklingen"); koerper = get ("koerper");
    schwebung = get ("schwebung"); anschlag = get ("anschlag"); vokal = get ("vokal"); jodelsprung = get ("jodelsprung");
    echo = get ("echo"); echoZeit = get ("echoZeit"); hall = get ("hall"); volume = get ("volume");
}

bool AlmProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AlmProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    ap.sr = sampleRate;
    juce::uint32 seed = 31;
    for (auto& v : voices.voices) v.setup (sampleRate, &ap, seed++);
    voices.reset();
    keyboardState.reset();
    reverb.prepare (sampleRate);
    wet.setSize (2, samplesPerBlock);
    for (auto& e : echoBuf) e.assign ((size_t) (sampleRate * 1.7) + 8, 0.0f);
    echoW = 0; echoLp[0] = echoLp[1] = 0.0f;
    gainSm.reset (sampleRate, 0.05);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    echoTimeSm.reset (sampleRate, 0.3);
    echoTimeSm.setCurrentAndTargetValue (echoZeit->load() * 0.001f * (float) sampleRate);
}

void AlmProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (wet.getNumSamples() < n) wet.setSize (2, n, false, false, true);

    ap.instrument = juce::jlimit (0, 3, (int) instrument->load());
    if (ap.instrument != lastInstrument) { voices.reset(); lastInstrument = ap.instrument; }
    ap.hell = hell->load(); ap.abklingen = abklingen->load(); ap.koerper = koerper->load(); ap.schwebung = schwebung->load();
    ap.anschlag = anschlag->load(); ap.vokal = vokal->load(); ap.jodelsprung = jodelsprung->load();

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    voices.process (L, R, n, midi);

    // Echo vom Berg: zwei Wände, links und rechts leicht versetzt, jedes Echo dunkler
    const float e = echo->load();
    echoTimeSm.setTargetValue (echoZeit->load() * 0.001f * (float) ap.sr);
    const int esz = (int) echoBuf[0].size();
    const float aLp = tmk::dsp::onePoleCoef (2500.0, ap.sr);
    for (int i = 0; i < n; ++i)
    {
        const float d = echoTimeSm.getNextValue();
        float outE[2];
        for (int c = 0; c < 2; ++c)
        {
            float rp = (float) echoW - d * (c == 0 ? 1.0f : 1.13f);
            while (rp < 0.0f) rp += (float) esz;
            const int i0 = (int) rp, i1 = (i0 + 1) % esz;
            const float fr = rp - (float) i0;
            const float y = echoBuf[c][(size_t) i0] + fr * (echoBuf[c][(size_t) i1] - echoBuf[c][(size_t) i0]);
            echoLp[c] = y + aLp * (echoLp[c] - y);
            outE[c] = echoLp[c];
        }
        echoBuf[0][(size_t) echoW] = L[i] + outE[1] * 0.45f;
        echoBuf[1][(size_t) echoW] = R[i] + outE[0] * 0.45f;
        if (++echoW >= esz) echoW = 0;
        L[i] += outE[0] * e * 0.8f;
        R[i] += outE[1] * e * 0.8f;
    }

    const float h = hall->load();
    tmk::FdnReverb::Settings rs;
    rs.sizeScale = 1.6f; rs.decay = 2.5f; rs.highRatio = 0.6f; rs.highCut = 5000.0f; rs.diffusion = 0.7f; rs.modDepth = 0.25f;
    reverb.update (rs);
    reverb.process (L, R, wet.getWritePointer (0), wet.getWritePointer (1), n);

    gainSm.setTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    const auto* wl = wet.getReadPointer (0); const auto* wr = wet.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSm.getNextValue();
        L[i] = std::tanh ((L[i] * (1.0f - 0.3f * h) + wl[i] * h) * g);
        R[i] = std::tanh ((R[i] * (1.0f - 0.3f * h) + wr[i] * h) * g);
    }
    scope.push (L, R, n);
}

juce::AudioProcessorEditor* AlmProcessor::createEditor() { return new AlmEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AlmProcessor(); }

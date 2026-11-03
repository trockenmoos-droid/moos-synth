#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace wolfgang;

//==============================================================================
void WolfgangVoice::start (bool wasActive)
{
    const auto& p = *params;
    // Glocke anschlagen: Teiltöne neu anregen (bei erneutem Anschlag addiert sich die Energie)
    const float hard = p.anschlag;
    const float vel = 0.35f + 0.65f * velocity;
    for (int k = 0; k < numBellPartials; ++k)
    {
        // Terz: 0 = Moll (6/5 der Prim), 1 = Dur (5/4 der Prim)
        float ratio = bellRatio[k];
        if (k == 2) ratio = 0.5f * (1.2f + 0.05f * p.terz);
        bInharm[k] = 1.0f + p.material * 0.035f * (rng.nextFloat() * 2.0f - 1.0f) * (k == 4 ? 0.2f : 1.0f);
        bDetune[k] = ratio * bInharm[k];
        // härterer Anschlag = mehr hohe Teiltöne
        const float tilt = std::pow (ratio / 1.0f, (hard - 0.5f) * 1.4f);
        const float a = bellAmp[k] * tilt * vel;
        bAmp[k] = wasActive ? juce::jmin (1.5f, bAmp[k] * 0.5f + a) : a;
        if (! wasActive) { bPhase[k][0] = rng.nextDouble(); bPhase[k][1] = rng.nextDouble(); }
        const float decaySec = juce::jmax (0.05f, p.groesse * bellDecay[k]);
        bCoef[k] = (float) std::exp (-6.9 / (decaySec * sr));      // -60 dB nach decaySec
    }
    bellGain = 1.0f;
    releaseCoef = 1.0f;
    damping = false;
    strikeNoise = 0.6f * hard * vel;
    strikeCoef = (float) std::exp (-1.0 / (0.004 * sr));
    strikeFilter.set (hz (note) * 3.0, sr, 1.2f);

    // Orgel
    if (! wasActive) for (auto& ph : oPhase) ph = rng.nextDouble();
    chiffEnv = 1.0f;
    chiffFilter.set (hz (note) * 2.0, sr, 3.0f);
    windFilter.set (1800.0, sr, 0.7f);
    organEnv.noteOn();
}

void WolfgangVoice::release()
{
    organEnv.noteOff();
    // Glocken klingen frei aus – nur mit "Dämpfen" werden sie beim Loslassen abgefangen
    if (params->daempfung > 0.001f)
    {
        damping = true;
        const float t = juce::jmax (0.05f, (1.0f - params->daempfung) * 6.0f + 0.05f);
        releaseCoef = (float) std::exp (-6.9 / (t * sr));
    }
}

void WolfgangVoice::render (float* L, float* R, int n, const tmk::PlayContext& ctx)
{
    const auto& p = *params;
    const double f0 = hz (note + ctx.bend);
    const float beat = p.schwebung * 1.6f;                         // Hz Unterschied der Paare
    double incB[numBellPartials][2];
    bool audibleB[numBellPartials];
    for (int k = 0; k < numBellPartials; ++k)
    {
        const double f = f0 * bDetune[k];
        audibleB[k] = f < 0.45 * sr && bAmp[k] > 1.0e-5f;
        incB[k][0] = (f - beat * 0.5) / sr;
        incB[k][1] = (f + beat * 0.5) / sr;
    }
    double incO[numRanks];
    float rankGain[numRanks];
    float rankSum = 0.0f;
    for (int r = 0; r < numRanks; ++r)
    {
        const double f = f0 * rankRatio[r];
        incO[r] = f / sr;
        rankGain[r] = (f < 0.42 * sr) ? p.ranks[r] : 0.0f;
        rankSum += rankGain[r];
    }
    const float organNorm = p.orgel * 0.22f / std::sqrt (juce::jmax (1.0f, rankSum));
    const float chiffDecay = (float) std::exp (-1.0 / (0.045 * sr));
    const float pan = juce::jlimit (-0.6f, 0.6f, (note - 60) / 40.0f);
    const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
    float peakB = 0.0f, peakO = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        // --- Glocke ---
        float bell = 0.0f;
        if (bellGain > 1.0e-5f && p.glocke > 0.0f)
        {
            for (int k = 0; k < numBellPartials; ++k)
            {
                if (! audibleB[k]) continue;
                bell += bAmp[k] * (tmk::dsp::sine (bPhase[k][0]) + tmk::dsp::sine (bPhase[k][1])) * 0.5f;
                bPhase[k][0] += incB[k][0]; if (bPhase[k][0] >= 1.0) bPhase[k][0] -= 1.0;
                bPhase[k][1] += incB[k][1]; if (bPhase[k][1] >= 1.0) bPhase[k][1] -= 1.0;
                bAmp[k] *= bCoef[k];
            }
            // Anschlag-Geräusch
            if (strikeNoise > 1.0e-4f)
            {
                bell += strikeFilter.bandpass ((rng.nextFloat() * 2.0f - 1.0f) * strikeNoise);
                strikeNoise *= strikeCoef;
            }
            if (damping) bellGain *= releaseCoef;
            bell *= bellGain * p.glocke * 0.24f;
        }

        // --- Orgel ---
        float organ = 0.0f;
        const float env = organEnv.getNextSample();
        if (env > 0.0f && p.orgel > 0.0f)
        {
            const float trem = 1.0f + p.tremulant * 0.35f * tmk::dsp::sine (tremPhase);
            tremPhase += p.tremPhaseInc; if (tremPhase >= 1.0) tremPhase -= 1.0;
            for (int r = 0; r < numRanks; ++r)
            {
                if (rankGain[r] <= 0.0f) continue;
                const double ph = oPhase[r];
                // Prinzipal-Pfeife: Grundton + etwas 2. und 3. Teilton
                float pipe = tmk::dsp::sine (ph) + 0.28f * tmk::dsp::sine (std::fmod (ph * 2.0, 1.0)) + 0.12f * tmk::dsp::sine (std::fmod (ph * 3.0, 1.0));
                if (r == numRanks - 1) pipe += 0.7f * tmk::dsp::sine (std::fmod (ph * 1.5, 1.0));   // Mixtur: Quinte dazu
                organ += pipe * rankGain[r];
                oPhase[r] += incO[r] * (1.0 + p.tremulant * 0.002 * (trem - 1.0)); if (oPhase[r] >= 1.0) oPhase[r] -= 1.0;
            }
            organ *= organNorm * trem;
            // Chiff: kurzes Anblasgeräusch, Wind: leises Rauschen
            const float noise = rng.nextFloat() * 2.0f - 1.0f;
            organ += chiffFilter.bandpass (noise) * chiffEnv * p.chiff * 0.25f * p.orgel;
            chiffEnv *= chiffDecay;
            organ += windFilter.lowpass (noise) * p.wind * 0.05f * p.orgel;
            organ *= env * (0.6f + 0.4f * velocity);
        }

        const float s = bell + organ;
        L[i] += s * gl;
        R[i] += s * gr;
        peakB = juce::jmax (peakB, std::abs (bell));
        peakO = juce::jmax (peakO, std::abs (organ));
    }
    bellLevelNow = peakB; organLevelNow = peakO;

    float maxAmp = 0.0f;
    for (float a : bAmp) maxAmp = juce::jmax (maxAmp, a);
    const bool bellDone = maxAmp * bellGain < 1.0e-4f || p.glocke <= 0.0f;
    if (bellDone && ! organEnv.isActive()) active = false;
}

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<P> (ID { "glocke", 1 },    "Glocke", 0.0f, 1.0f, 0.8f));
    p.push_back (std::make_unique<P> (ID { "groesse", 1 },   u8 ("Gr\xc3\xb6\xc3\x9f" "e (s)"), tmk::skewed (0.5f, 30.0f, 6.0f), 7.0f));
    p.push_back (std::make_unique<P> (ID { "terz", 1 },      "Terz Moll/Dur", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P> (ID { "material", 1 },  "Material", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P> (ID { "schwebung", 1 }, "Schwebung", 0.0f, 1.0f, 0.4f));
    p.push_back (std::make_unique<P> (ID { "anschlag", 1 },  "Anschlag", 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P> (ID { "daempfung", 1 }, u8 ("D\xc3\xa4mpfen"), 0.0f, 1.0f, 0.0f));
    const char* rankNames[numRanks] = { "16'", "8'", "4'", "2 2/3'", "2'", "1 3/5'", "Mixtur" };
    const float rankDef[numRanks] = { 0.0f, 0.8f, 0.5f, 0.0f, 0.3f, 0.0f, 0.0f };
    for (int r = 0; r < numRanks; ++r)
        p.push_back (std::make_unique<P> (ID { rankIds[r], 1 }, juce::String ("Register ") + rankNames[r], 0.0f, 1.0f, rankDef[r]));
    p.push_back (std::make_unique<P> (ID { "orgel", 1 },     "Orgel", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P> (ID { "chiff", 1 },     "Chiff", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<P> (ID { "wind", 1 },      "Wind", 0.0f, 1.0f, 0.15f));
    p.push_back (std::make_unique<P> (ID { "tremulant", 1 }, "Tremulant", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<P> (ID { "oAttack", 1 },   "Orgel Attack (s)", tmk::skewed (0.005f, 2.0f, 0.1f), 0.04f));
    p.push_back (std::make_unique<P> (ID { "oRelease", 1 },  "Orgel Release (s)", tmk::skewed (0.02f, 4.0f, 0.4f), 0.25f));
    p.push_back (std::make_unique<P> (ID { "hall", 1 },      "Kirchenhall", 0.0f, 1.0f, 0.45f));
    p.push_back (std::make_unique<P> (ID { "hallZeit", 1 },  "Hallzeit (s)", tmk::skewed (1.0f, 12.0f, 4.0f), 5.0f));
    p.push_back (std::make_unique<P> (ID { "volume", 1 },    u8 ("Lautst\xc3\xa4rke (dB)"), -48.0f, 6.0f, -6.0f));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Pfarrkirchen-Glocke", {} },
        { "Wolfgangsee-Glocke",  { { "groesse", 18.0f }, { "schwebung", 0.6f }, { "anschlag", 0.35f }, { "hall", 0.6f }, { "hallZeit", 7.0f } } },
        { "Dur-Gel\xc3\xa4ut",   { { "terz", 1.0f }, { "groesse", 9.0f }, { "material", 0.15f } } },
        { "Kleines Gl\xc3\xb6" "ckchen", { { "groesse", 2.0f }, { "anschlag", 0.9f }, { "schwebung", 0.2f }, { "material", 0.5f } } },
        { "Prinzipal 8' + 4'",   { { "glocke", 0.0f }, { "orgel", 0.9f }, { "r8", 0.9f }, { "r4", 0.6f }, { "r2", 0.0f }, { "hall", 0.5f } } },
        { "Volles Werk",         { { "glocke", 0.0f }, { "orgel", 1.0f }, { "r16", 0.8f }, { "r8", 1.0f }, { "r4", 0.9f }, { "r223", 0.6f },
                                   { "r2", 0.8f }, { "r135", 0.4f }, { "mixtur", 0.7f }, { "hall", 0.55f }, { "hallZeit", 6.0f } } },
        { "Fl\xc3\xb6te mit Tremulant", { { "glocke", 0.0f }, { "orgel", 0.8f }, { "r8", 0.7f }, { "r4", 0.4f }, { "r2", 0.0f },
                                   { "chiff", 0.6f }, { "tremulant", 0.6f }, { "oAttack", 0.08f } } },
        { "Glocke und Orgel",    { { "orgel", 0.6f }, { "r8", 0.7f }, { "r4", 0.4f }, { "r2", 0.2f }, { "glocke", 0.6f }, { "hall", 0.55f } } },
        { "Pilgerweg",           { { "glocke", 0.5f }, { "groesse", 14.0f }, { "orgel", 0.45f }, { "r16", 0.5f }, { "r8", 0.6f }, { "r4", 0.0f },
                                   { "r2", 0.0f }, { "oAttack", 1.2f }, { "oRelease", 2.5f }, { "hall", 0.75f }, { "hallZeit", 9.0f }, { "tremulant", 0.25f } } },
    };
}

//==============================================================================
WolfgangProcessor::WolfgangProcessor()
    : tmk::Processor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "SANKT WOLFGANG", factoryPresets())
{
    auto get = [this] (const char* i) { return apvts.getRawParameterValue (i); };
    glocke = get ("glocke"); groesse = get ("groesse"); terz = get ("terz"); material = get ("material");
    schwebung = get ("schwebung"); anschlag = get ("anschlag"); daempfung = get ("daempfung");
    for (int r = 0; r < numRanks; ++r) ranks[r] = get (rankIds[r]);
    orgel = get ("orgel"); chiff = get ("chiff"); wind = get ("wind"); tremulant = get ("tremulant");
    oAttack = get ("oAttack"); oRelease = get ("oRelease"); hall = get ("hall"); hallZeit = get ("hallZeit"); volume = get ("volume");
}

bool WolfgangProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WolfgangProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    wp.sr = sampleRate;
    juce::uint32 seed = 17;
    for (auto& v : voices.voices) { v.setup (sampleRate, &wp, seed++); v.organEnv.setSampleRate (sampleRate); }
    voices.reset();
    keyboardState.reset();
    reverb.prepare (sampleRate);
    wet.setSize (2, samplesPerBlock);
    gainSm.reset (sampleRate, 0.05);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
}

void WolfgangProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (wet.getNumSamples() < n) wet.setSize (2, n, false, false, true);

    wp.glocke = glocke->load(); wp.groesse = groesse->load(); wp.terz = terz->load(); wp.material = material->load();
    wp.schwebung = schwebung->load(); wp.anschlag = anschlag->load(); wp.daempfung = daempfung->load();
    for (int r = 0; r < numRanks; ++r) wp.ranks[r] = ranks[r]->load();
    // Mod Wheel öffnet zusätzlich Register (Schwellwerk), Aftertouch verstärkt den Tremulant
    wp.orgel = juce::jlimit (0.0f, 1.0f, orgel->load() * (1.0f + 0.5f * voices.ctx.modWheel));
    wp.chiff = chiff->load(); wp.wind = wind->load();
    wp.tremulant = juce::jlimit (0.0f, 1.0f, tremulant->load() + voices.ctx.aftertouch * 0.6f);
    wp.tremPhaseInc = (float) (5.6 / wp.sr);
    const juce::ADSR::Parameters ep { oAttack->load(), 0.05f, 1.0f, oRelease->load() };
    for (auto& v : voices.voices) v.organEnv.setParameters (ep);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    voices.process (L, R, n, midi);

    float bl = 0.0f, ol = 0.0f;
    for (auto& v : voices.voices) if (v.active) { bl = juce::jmax (bl, v.bellLevelNow); ol = juce::jmax (ol, v.organLevelNow); }
    bellLevel = juce::jmax (bl, bellLevel.load() * 0.9f);
    organLevel = juce::jmax (ol, organLevel.load() * 0.9f);

    const float h = hall->load();
    tmk::FdnReverb::Settings rs;
    rs.sizeScale = 2.6f; rs.decay = hallZeit->load(); rs.bassRatio = 1.2f; rs.highRatio = 0.5f; rs.highCut = 4500.0f;
    rs.modDepth = 0.2f; rs.modRate = 0.3f; rs.diffusion = 0.85f; rs.width = 1.0f;
    reverb.update (rs);
    reverb.process (L, R, wet.getWritePointer (0), wet.getWritePointer (1), n);

    gainSm.setTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    const auto* wl = wet.getReadPointer (0); const auto* wr = wet.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSm.getNextValue();
        L[i] = std::tanh ((L[i] * (1.0f - 0.45f * h) + wl[i] * h * 1.2f) * g);
        R[i] = std::tanh ((R[i] * (1.0f - 0.45f * h) + wr[i] * h * 1.2f) * g);
    }
}

juce::AudioProcessorEditor* WolfgangProcessor::createEditor() { return new WolfgangEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WolfgangProcessor(); }

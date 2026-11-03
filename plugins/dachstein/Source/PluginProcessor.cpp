#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dachstein;

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat; using PI = juce::AudioParameterInt; using ID = juce::ParameterID;
    auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<P>  (ID { "obertoene", 1 }, u8 ("Obert\xc3\xb6ne"), 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "kristall", 1 },  "Kristall", 0.0f, 1.0f, 0.25f));
    p.push_back (std::make_unique<P>  (ID { "spalten", 1 },   "Spalten", 0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<PI> (ID { "teiltoene", 1 }, u8 ("Teilt\xc3\xb6ne"), 4, maxPartials, 24));
    p.push_back (std::make_unique<P>  (ID { "bewegung", 1 },  "Bewegung", 0.0f, 1.0f, 0.5f));
    p.push_back (std::make_unique<P>  (ID { "tempo", 1 },     "Tempo (Hz)", tmk::skewed (0.02f, 3.0f, 0.3f), 0.15f));
    p.push_back (std::make_unique<P>  (ID { "schmelze", 1 },  "Schmelze", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "breite", 1 },    "Breite", 0.0f, 1.0f, 0.8f));
    p.push_back (std::make_unique<P>  (ID { "fels", 1 },      "Fels", 0.0f, 1.0f, 0.3f));
    p.push_back (std::make_unique<P>  (ID { "wind", 1 },      "Wind", 0.0f, 1.0f, 0.2f));
    p.push_back (std::make_unique<P>  (ID { "windFarbe", 1 }, "Windfarbe (Hz)", tmk::skewed (200.0f, 8000.0f, 1500.0f), 1500.0f));
    p.push_back (std::make_unique<P>  (ID { "boeen", 1 },     u8 ("B\xc3\xb6" "en"), 0.0f, 1.0f, 0.4f));
    p.push_back (std::make_unique<P>  (ID { "knistern", 1 },  "Knistern", 0.0f, 1.0f, 0.15f));
    p.push_back (std::make_unique<P>  (ID { "attack", 1 },    "Attack (s)", tmk::skewed (0.01f, 20.0f, 2.0f), 3.0f));
    p.push_back (std::make_unique<P>  (ID { "release", 1 },   "Release (s)", tmk::skewed (0.05f, 30.0f, 4.0f), 6.0f));
    p.push_back (std::make_unique<P>  (ID { "hall", 1 },      "Hall", 0.0f, 1.0f, 0.45f));
    p.push_back (std::make_unique<P>  (ID { "volume", 1 },    u8 ("Lautst\xc3\xa4rke (dB)"), -48.0f, 6.0f, -8.0f));
    return { p.begin(), p.end() };
}

static std::vector<tmk::Preset> factoryPresets()
{
    return {
        { "Gletschermorgen", {} },
        { "Hoher Dachstein",   { { "obertoene", 0.72f }, { "kristall", 0.5f }, { "bewegung", 0.6f }, { "hall", 0.6f }, { "attack", 4.0f } } },
        { "Eiskapelle",        { { "kristall", 0.85f }, { "spalten", 0.3f }, { "obertoene", 0.6f }, { "attack", 0.4f }, { "release", 9.0f },
                                 { "bewegung", 0.35f }, { "hall", 0.65f }, { "knistern", 0.1f } } },
        { "F\xc3\xb6hnsturm", { { "wind", 0.85f }, { "boeen", 0.9f }, { "windFarbe", 900.0f }, { "obertoene", 0.3f }, { "bewegung", 0.8f },
                                 { "tempo", 0.6f }, { "hall", 0.4f } } },
        { "Gjaidalm im Nebel", { { "obertoene", 0.22f }, { "schmelze", 0.75f }, { "wind", 0.3f }, { "windFarbe", 600.0f }, { "kristall", 0.1f },
                                 { "attack", 5.0f }, { "release", 10.0f }, { "hall", 0.55f } } },
        { "Spaltenzone",       { { "spalten", 1.0f }, { "knistern", 0.55f }, { "kristall", 0.35f }, { "obertoene", 0.55f }, { "bewegung", 0.7f } } },
        { "Tiefer Firn",       { { "fels", 0.85f }, { "obertoene", 0.15f }, { "attack", 6.0f }, { "teiltoene", 12 }, { "wind", 0.1f }, { "hall", 0.35f } } },
        { "Riesen-Eish\xc3\xb6hle", { { "hall", 1.0f }, { "knistern", 0.35f }, { "bewegung", 0.65f }, { "tempo", 0.05f }, { "kristall", 0.45f },
                                 { "release", 15.0f } } },
        { "Schmelzwasser",     { { "tempo", 1.6f }, { "bewegung", 0.9f }, { "schmelze", 0.5f }, { "obertoene", 0.6f }, { "wind", 0.15f },
                                 { "attack", 1.0f } } },
    };
}

DachsteinProcessor::DachsteinProcessor()
    : tmk::Processor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), "DACHSTEIN", factoryPresets())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    obertoene = get ("obertoene"); kristall = get ("kristall"); spalten = get ("spalten"); teiltoene = get ("teiltoene");
    bewegung = get ("bewegung"); tempo = get ("tempo"); schmelze = get ("schmelze"); breite = get ("breite");
    fels = get ("fels"); wind = get ("wind"); windFarbe = get ("windFarbe"); boeen = get ("boeen");
    knistern = get ("knistern"); attack = get ("attack"); release = get ("release"); hall = get ("hall"); volume = get ("volume");

    sine.resize (sineSize + 1);
    for (int i = 0; i <= sineSize; ++i) sine[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / sineSize);

    juce::Random r (1857);   // Erstbesteigung des Hohen Dachsteins: 1832 – nah genug :)
    for (int k = 0; k < maxPartials; ++k)
    {
        const float p = (k == 0) ? 0.0f : (r.nextFloat() * 2.0f - 1.0f);
        pan[k][0] = p; pan[k][1] = p;
        detuneSeed[k] = r.nextFloat();
    }
}

bool DachsteinProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DachsteinProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& v : voices) { v.active = false; v.env.setSampleRate (sr); }
    keyboardState.reset();
    reverb.prepare (sr);
    wet.setSize (2, juce::jmax (samplesPerBlock, maxChunk));
    gainSm.reset (sr, 0.05);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    windLp = gust = gustTarget = act = 0.0f;
    for (int c = 0; c < 2; ++c) windIc1[c] = windIc2[c] = 0.0f;
    for (auto& p : pings) p = {};
}

void DachsteinProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        Voice* t = nullptr;
        for (auto& v : voices) if (v.active && v.note == m.getNoteNumber()) { t = &v; break; }
        if (t == nullptr) for (auto& v : voices) if (! v.active) { t = &v; break; }
        if (t == nullptr)
        {
            juce::uint64 best = std::numeric_limits<juce::uint64>::max();
            for (auto& v : voices) if (! v.held && v.age < best) { best = v.age; t = &v; }
            if (t == nullptr) for (auto& v : voices) if (v.age < best) { best = v.age; t = &v; }
        }
        if (! t->active)
        {
            for (int k = 0; k < maxPartials; ++k)
            {
                t->phase[k][0] = rng.nextDouble(); t->phase[k][1] = rng.nextDouble();
                t->drift[k] = t->driftTarget[k] = rng.nextFloat();
            }
            t->subPhase = 0.0;
            t->env.reset();
        }
        t->active = true; t->held = true; t->sustained = false;
        t->note = m.getNoteNumber(); t->velocity = m.getFloatVelocity(); t->age = ++ageCounter;
        t->env.noteOn();
    }
    else if (m.isNoteOff())
    {
        for (auto& v : voices)
            if (v.active && v.held && v.note == m.getNoteNumber())
            {
                v.held = false;
                if (sustainPedal) v.sustained = true; else v.env.noteOff();
            }
    }
    else if (m.isSustainPedalOn()) sustainPedal = true;
    else if (m.isSustainPedalOff())
    {
        sustainPedal = false;
        for (auto& v : voices) if (v.sustained && ! v.held) { v.sustained = false; v.env.noteOff(); }
    }
    else if (m.isPitchWheel())      bend = (float) ((m.getPitchWheelValue() - 8192) / 8192.0) * 2.0f;
    else if (m.isController() && m.getControllerNumber() == 1) modWheel = m.getControllerValue() / 127.0f;
    else if (m.isChannelPressure()) aftertouch = m.getChannelPressureValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        for (auto& v : voices) { v.held = v.sustained = false; v.env.noteOff(); }
}

void DachsteinProcessor::renderVoices (float* L, float* R, int start, int num)
{
    constexpr int sub = 32;
    const float width = breite->load();
    const float sub0  = fels->load();
    float lastAmp[maxPartials] {};
    juce::uint64 newest = 0;

    for (int s0 = 0; s0 < num; s0 += sub)
    {
        const int n = juce::jmin (sub, num - s0);
        float maxEnv = 0.0f;

        for (auto& v : voices)
        {
            if (! v.active) continue;
            const double f0 = 440.0 * std::pow (2.0, (v.note - 69 + bend) / 12.0);
            float tl[sub] {}, tr[sub] {};

            // Bewegung: jeder Teilton wandert langsam zu einem neuen Zufallsziel
            const float smooth = 1.0f - std::exp (-(float) n * moveRate / (float) sr * 2.0f);
            const float change = (float) n * moveRate / (float) sr;
            for (int k = 0; k < numPartials; ++k)
            {
                if (rng.nextFloat() < change) v.driftTarget[k] = rng.nextFloat();
                v.drift[k] += (v.driftTarget[k] - v.drift[k]) * smooth;
            }

            for (int k = 0; k < numPartials; ++k)
            {
                const double f = f0 * ratio[k];
                if (f > 0.45 * sr) continue;
                const float fade = f > 0.35 * sr ? (float) ((0.45 * sr - f) / (0.1 * sr)) : 1.0f;
                const float a = amp[k] * ampNorm * (1.0f - movement * v.drift[k]) * fade;
                if (v.age >= newest) lastAmp[k] = amp[k] * (1.0f - movement * v.drift[k]);
                const float pl = std::sqrt (0.5f * (1.0f - pan[k][0] * width)), pr = std::sqrt (0.5f * (1.0f + pan[k][0] * width));
                const float cents = melt * 14.0f * (0.4f + 0.6f * detuneSeed[k]);
                const int copies = melt > 0.01f ? 2 : 1;
                const float ca = a / std::sqrt ((float) copies);
                for (int c = 0; c < copies; ++c)
                {
                    const double inc = f * std::pow (2.0, (c == 0 ? cents : -cents) / 1200.0) / sr;
                    double ph = v.phase[k][c];
                    // zweite Kopie leicht anders im Stereobild
                    const float gl = ca * (c == 0 ? pl : pr), gr = ca * (c == 0 ? pr : pl);
                    for (int i = 0; i < n; ++i)
                    {
                        const double idx = ph * sineSize;
                        const int ii = (int) idx;
                        const float fr = (float) (idx - ii);
                        const float s = sine[(size_t) ii] + fr * (sine[(size_t) ii + 1] - sine[(size_t) ii]);
                        tl[i] += s * gl; tr[i] += s * gr;
                        ph += inc; if (ph >= 1.0) ph -= 1.0;
                    }
                    v.phase[k][c] = ph;
                }
            }
            if (v.age >= newest) newest = v.age;

            // Fels: Sub-Oktave, leicht gesättigt
            const double subInc = f0 * 0.5 / sr;
            const float velGain = 0.4f + 0.6f * v.velocity;
            for (int i = 0; i < n; ++i)
            {
                const float subS = sub0 > 0.001f ? std::tanh (1.5f * (float) std::sin (v.subPhase * juce::MathConstants<double>::twoPi)) * sub0 * 0.22f : 0.0f;
                v.subPhase += subInc; if (v.subPhase >= 1.0) v.subPhase -= 1.0;
                const float e = v.env.getNextSample() * velGain;
                maxEnv = juce::jmax (maxEnv, e);
                L[start + s0 + i] += (tl[i] + subS) * e;
                R[start + s0 + i] += (tr[i] + subS) * e;
            }
            if (! v.env.isActive()) { v.active = v.held = v.sustained = false; }
        }

        // Aktivität (für Wind & Knistern): folgt dem lautesten Ton, langsam
        const float target = juce::jmin (1.0f, maxEnv * 1.5f);
        act += (target - act) * (1.0f - std::exp (-(float) n / (0.4f * (float) sr)));
    }

    if (newest > 0)
        for (int k = 0; k < maxPartials; ++k)
            partialAmp[k] = k < numPartials && amp[0] > 0.0f ? lastAmp[k] / amp[0] : 0.0f;
    else
        for (int k = 0; k < maxPartials; ++k)
            partialAmp[k] = k < numPartials && amp[0] > 0.0f ? amp[k] / amp[0] * (1.0f - 0.5f * movement) : 0.0f;
    activity = act;
}

void DachsteinProcessor::renderAmbience (float* L, float* R, int start, int n)
{
    const float windAmt = wind->load(), gusts = boeen->load(), crackle = knistern->load();
    const float centre = windFarbe->load();
    if (act < 1.0e-4f && windAmt <= 0.0f && crackle <= 0.0f) return;

    const float gustSmooth = 1.0f - std::exp (-1.0f / (0.9f * (float) sr));
    const float pTrigger = crackle * crackle * 0.0012f;

    for (int i = 0; i < n; ++i)
    {
        // Böen: langsamer Zufallsweg
        if (rng.nextFloat() < 1.0f / (float) (sr * 1.4)) gustTarget = rng.nextFloat();
        gust += (gustTarget - gust) * gustSmooth;

        const float fc = juce::jlimit (80.0f, (float) (0.42 * sr), centre * (0.6f + 0.9f * gust * gusts + 0.2f));
        const float g  = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
        const float k  = 0.8f;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float level = windAmt * act * 0.35f * ((1.0f - gusts) * 0.7f + gusts * gust * 1.5f);

        float out[2];
        for (int c = 0; c < 2; ++c)
        {
            const float x = rng.nextFloat() * 2.0f - 1.0f;
            const float v3 = x - windIc2[c];
            const float v1 = a1 * windIc1[c] + a2 * v3;
            const float v2 = windIc2[c] + a2 * windIc1[c] + a3 * v3;
            windIc1[c] = 2.0f * v1 - windIc1[c];
            windIc2[c] = 2.0f * v2 - windIc2[c];
            out[c] = k * v1 * level;
        }

        // Eisknistern: kurze, helle Resonanzen
        if (crackle > 0.0f && rng.nextFloat() < pTrigger * act)
        {
            auto& p = pings[nextPing];
            nextPing = (nextPing + 1) % 6;
            const float f = 2200.0f + rng.nextFloat() * 7000.0f;
            const float decay = 0.003f + rng.nextFloat() * 0.025f;
            const float rr = std::exp (-1.0f / (decay * (float) sr));
            p.b1 = 2.0f * rr * (float) std::cos (juce::MathConstants<double>::twoPi * juce::jmin ((double) f, 0.45 * sr) / sr);
            p.b2 = -rr * rr;
            const float a = crackle * (0.02f + 0.08f * rng.nextFloat());
            const float pp = rng.nextFloat();
            p.gainL = a * std::sqrt (1.0f - pp); p.gainR = a * std::sqrt (pp);
            p.y1 += 1.0f;
        }
        for (auto& p : pings)
        {
            const float y = p.b1 * p.y1 + p.b2 * p.y2;
            p.y2 = p.y1; p.y1 = y;
            out[0] += y * p.gainL; out[1] += y * p.gainR;
        }

        L[start + i] += out[0];
        R[start + i] += out[1];
    }
}

void DachsteinProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (wet.getNumSamples() < n) wet.setSize (2, n, false, false, true);

    // Spektrum für diesen Block
    numPartials = juce::jlimit (4, maxPartials, (int) std::round (teiltoene->load()));
    const float bright = juce::jlimit (0.0f, 1.0f, obertoene->load() + 0.5f * modWheel);
    const float B = std::pow (kristall->load(), 2.0f) * 0.0025f;
    const float split = spalten->load();
    float sumSq = 0.0f;
    for (int k = 0; k < maxPartials; ++k)
    {
        const float h = (float) (k + 1);
        ratio[k] = h * std::sqrt (1.0f + B * h * h);
        amp[k] = k < numPartials ? std::pow (h, -(2.2f - 1.9f * bright)) * ((k % 2 == 1) ? 1.0f - split : 1.0f) : 0.0f;
        sumSq += amp[k] * amp[k];
        partialRatio[k] = ratio[k];
    }
    ampNorm  = 0.22f / std::sqrt (juce::jmax (1.0e-6f, sumSq));
    movement = juce::jlimit (0.0f, 1.0f, bewegung->load() + 0.5f * aftertouch);
    moveRate = tempo->load();
    melt     = schmelze->load();

    const juce::ADSR::Parameters ep { attack->load(), 0.1f, 1.0f, release->load() };
    for (auto& v : voices) v.env.setParameters (ep);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    int pos = 0;
    for (const auto m : midi)
    {
        const int t = juce::jlimit (0, n, m.samplePosition);
        if (t > pos) { renderVoices (L, R, pos, t - pos); pos = t; }
        handleMidi (m.getMessage());
    }
    if (pos < n) renderVoices (L, R, pos, n - pos);
    renderAmbience (L, R, 0, n);

    // Hall
    const float h = hall->load();
    tmk::FdnReverb::Settings rs;
    rs.sizeScale = 2.3f; rs.decay = 4.0f + 8.0f * h; rs.highRatio = 0.6f; rs.highCut = 6000.0f;
    rs.bassRatio = 1.0f; rs.modDepth = 0.35f; rs.modRate = 0.25f; rs.diffusion = 0.8f; rs.width = 1.0f;
    reverb.update (rs);
    reverb.process (L, R, wet.getWritePointer (0), wet.getWritePointer (1), n);

    gainSm.setTargetValue (juce::Decibels::decibelsToGain (volume->load(), -48.0f));
    const auto* wl = wet.getReadPointer (0); const auto* wr = wet.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSm.getNextValue();
        L[i] = std::tanh ((L[i] * (1.0f - 0.4f * h) + wl[i] * h * 1.3f) * g * 2.0f) * 0.5f;
        R[i] = std::tanh ((R[i] * (1.0f - 0.4f * h) + wr[i] * h * 1.3f) * g * 2.0f) * 0.5f;
    }
}

juce::AudioProcessorEditor* DachsteinProcessor::createEditor() { return new DachsteinEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DachsteinProcessor(); }

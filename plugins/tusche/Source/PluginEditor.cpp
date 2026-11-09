#include "PluginEditor.h"
#include "TuscheAssets.h"

using namespace tusche;

static juce::Image loadAsset (const char* data, int size) { return juce::ImageFileFormat::loadFrom (data, (size_t) size); }

//==============================================================================
// InkDisplay
InkDisplay::InkDisplay (TuscheProcessor& p) : proc (p)
{
    startTimerHz (30);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void InkDisplay::rebuildPeaks()
{
    const auto* src = proc.currentSource();
    const int w = juce::jmax (1, (int) waveArea().getWidth());
    peaks.assign ((size_t) w, 0.0f);
    if (src == nullptr) return;
    const int L = src->data.getNumSamples();
    const float* d = src->data.getReadPointer (0);
    for (int x = 0; x < w; ++x)
    {
        const int a = (int) ((juce::int64) x * L / w), b = juce::jmax (a + 1, (int) ((juce::int64) (x + 1) * L / w));
        float m = 0.0f;
        for (int i = a; i < b; i += 4) m = juce::jmax (m, std::abs (d[i]));
        peaks[(size_t) x] = m;
    }
    peaksFor = proc.sourceChanges.load(); peaksWidth = w; peaksSource = src;
}

void InkDisplay::paint (juce::Graphics& g)
{
    using namespace ink;
    auto r = getLocalBounds().toFloat();
    juce::Random rnd (31);
    const int bi = proc.currentSheetIndex();
    const auto* sh = proc.sheet (bi);

    // Druckblock mit ausgefranstem Rand, darin das Blatt (ganz, damit der Lesekopf genau über der gelesenen Stelle steht)
    auto block = roughRect (r.reduced (3.0f), rnd, 3.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (block);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        if (sh != nullptr) g.drawImage (sh->image, r, juce::RectanglePlacement::stretchToFit);
        g.setColour (ink::ink.withAlpha (0.52f));
        g.fillRect (r);
        // Lesekopf der Partitur
        const float head = proc.readHead.load();
        if (head >= 0.0f)
        {
            const float hx = r.getX() + head * r.getWidth();
            g.setGradientFill (juce::ColourGradient (green.withAlpha (0.0f), hx - 40.0f, 0.0f, green.withAlpha (0.28f), hx, 0.0f, false));
            g.fillRect (juce::Rectangle<float> (hx - 40.0f, r.getY(), 40.0f, r.getHeight()));
            juce::Path hp; hp.startNewSubPath (hx, r.getY() + 4.0f); hp.lineTo (hx + 1.5f, r.getBottom() - 4.0f);
            juce::Random hr (17);
            dryStroke (g, hp, 4.0f, green, hr, 0.4f);
        }
    }

    // Wellenform in Papierweiß – Spalte für Spalte, wie ein trockener Pinsel
    const auto wa = waveArea();
    if (peaksFor != proc.sourceChanges.load() || peaksWidth != (int) wa.getWidth() || peaksSource != proc.currentSource()) rebuildPeaks();
    const float mid = wa.getCentreY(), hh = wa.getHeight() * 0.46f;
    juce::Random wr (7);
    for (int x = 0; x < (int) peaks.size(); ++x)
    {
        const float pk = peaks[(size_t) x] * hh;
        if (wr.nextFloat() < 0.08f) continue;                                  // Aussparung
        g.setColour (paper.withAlpha ((0.55f + 0.4f * wr.nextFloat()) * (proc.readHead.load() >= 0.0f ? 0.45f : 1.0f)));
        g.fillRect (wa.getX() + x, mid - pk - wr.nextFloat() * 1.5f, 1.0f, pk * 2.0f + 1.0f);
    }

    // Striche: Bleistiftmarken; gewählter Strich grün, Breite = Korngröße
    const int sel = juce::jlimit (0, numChops - 1, (int) proc.apvts.getRawParameterValue ("auswahl")->load() - 1);
    const auto* src = proc.currentSource();
    const double lenSec = src != nullptr ? src->data.getNumSamples() / src->rate : 8.0;
    for (int c = 0; c < numChops; ++c)
    {
        const float pos = proc.apvts.getRawParameterValue (cid (c, "pos"))->load();
        const bool on = proc.apvts.getRawParameterValue (cid (c, "an"))->load() > 0.5f;
        const float x = wa.getX() + pos * wa.getWidth();
        if (c == sel)
        {
            const float kw = juce::jmax (5.0f, (float) (proc.apvts.getRawParameterValue (cid (c, "korn"))->load() * 0.001 / lenSec) * wa.getWidth());
            juce::Random gr (c + 100);
            stamp (g, juce::Rectangle<float> (x, wa.getY() - 4.0f, kw, wa.getHeight() + 8.0f), green.withAlpha (0.42f), gr, 0.6f);
        }
        const float glow = proc.chopGlow[c].load();
        g.setColour ((c == sel ? green : paper).withAlpha (on ? 0.55f + 0.45f * glow : 0.25f));
        g.drawLine (x, wa.getY() - 6.0f, x, wa.getY() + 4.0f, 1.2f);
        g.drawLine (x, wa.getBottom() - 4.0f, x, wa.getBottom() + 6.0f, 1.2f);
        pencilText (g, juce::String (c + 1), { x - 10.0f, wa.getBottom() + 5.0f, 20.0f, 13.0f }, juce::Justification::centred, 9.5f,
                    (c == sel ? green : paper).withAlpha (on ? 0.85f : 0.35f));
    }

    // Körner: weiße Tupfer
    for (int i = 0; i < maxGrains; ++i)
    {
        const float glow = proc.grainView[i].glow.load();
        if (glow < 0.05f) continue;
        const float x = wa.getX() + proc.grainView[i].pos.load() * wa.getWidth();
        const float y = mid - proc.grainView[i].pan.load() * hh * 0.7f;
        const float sz = 3.0f + 9.0f * glow;
        juce::Random dr (i * 13);
        g.setColour (paper.withAlpha (0.35f + 0.6f * glow));
        g.fillEllipse (x - sz * 0.5f, y - sz * 0.35f, sz, sz * (0.5f + 0.4f * dr.nextFloat()));
        g.setColour (paper.withAlpha (0.4f * glow));
        g.fillEllipse (x + sz * 0.4f, y + sz * 0.2f, sz * 0.25f, sz * 0.2f);
    }

    // Kopfzeile im Block
    pencilText (g, (bi >= numSheets ? juce::String ("EIGENES BLATT") : "BLATT " + juce::String (bi + 1) + "/5") + juce::String::fromUTF8 ("  \xe2\x80\xba"),
                blattButton(), juce::Justification::centredLeft, 11.0f, paper.withAlpha (0.85f));
    pencilText (g, juce::String::fromUTF8 ("LADEN \xe2\x80\xa6"), loadButton(), juce::Justification::centredRight, 11.0f, paper.withAlpha (0.85f));
    if (src != nullptr)
        pencilText (g, src->name, { r.getCentreX() - 160.0f, 6.0f, 320.0f, 20.0f }, juce::Justification::centred, 12.0f, paper);
    pencilText (g, juce::String::fromUTF8 ("Sample oder Bild hierher ziehen \xc2\xb7 klicken/ziehen setzt die Position des Strichs"),
                { 14.0f, r.getBottom() - 17.0f, r.getWidth() - 28.0f, 14.0f }, juce::Justification::centredRight, 9.5f, paper.withAlpha (0.55f));
    if (dragOver)
    {
        g.setColour (green.withAlpha (0.25f));
        g.fillPath (block);
        g.setColour (green);
        g.strokePath (block, juce::PathStrokeType (2.0f));
    }
}

void InkDisplay::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (blattButton().contains (p))
    {
        const int count = numSheets + (proc.sheet (numSheets) != nullptr ? 1 : 0);
        proc.setSheet ((proc.currentSheetIndex() + (e.mods.isRightButtonDown() ? count - 1 : 1)) % count);
        return;
    }
    if (loadButton().contains (p))
    {
        chooser = std::make_unique<juce::FileChooser> ("Sample laden", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File()) proc.loadSample (fc.getResult());
        });
        return;
    }
    if (waveArea().expanded (0.0f, 20.0f).contains (p) && onPositionDrag)
    {
        draggingPos = true;
        if (onGesture) onGesture (true);
        onPositionDrag (juce::jlimit (0.0f, 1.0f, (p.x - waveArea().getX()) / waveArea().getWidth()));
    }
}

void InkDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingPos && onPositionDrag)
        onPositionDrag (juce::jlimit (0.0f, 1.0f, (e.position.x - waveArea().getX()) / waveArea().getWidth()));
}

void InkDisplay::mouseUp (const juce::MouseEvent&)
{
    if (draggingPos && onGesture) onGesture (false);
    draggingPos = false;
}

bool InkDisplay::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (TuscheProcessor::isAudioFile (juce::File (f)) || TuscheProcessor::isImageFile (juce::File (f))) return true;
    return false;
}

void InkDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    // Bilder werden zum Blatt (Partitur), Audiodateien zum Sample
    for (auto& f : files)
        if (TuscheProcessor::isImageFile (juce::File (f))) { proc.loadSheetImage (juce::File (f)); break; }
    for (auto& f : files)
        if (TuscheProcessor::isAudioFile (juce::File (f)) && proc.loadSample (juce::File (f)).isEmpty()) break;
}

//==============================================================================
// StepRow
juce::Rectangle<float> StepRow::cell (int i) const
{
    const float w = (float) getWidth() / numChops;
    return juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (4.0f, 6.0f);
}

void StepRow::paint (juce::Graphics& g)
{
    using namespace ink;
    const int sel = selected(), cur = proc.currentStep.load();
    for (int i = 0; i < numChops; ++i)
    {
        juce::Random rnd (i * 977 + 3);
        auto r = cell (i);
        const bool on = proc.apvts.getRawParameterValue (cid (i, "an"))->load() > 0.5f;
        if (i == sel)
        {
            juce::Path smear;
            smear.startNewSubPath (r.getX() - 2.0f, r.getBottom() + 3.0f);
            smear.lineTo (r.getRight() + 2.0f, r.getBottom() + 1.0f);
            juce::Random gr (i);
            dryStroke (g, smear, 6.0f, green, gr, 0.8f);
        }
        if (on)
        {
            const float glow = proc.chopGlow[i].load();
            stamp (g, r, ink::ink.interpolatedWith (graphite, 0.35f * glow), rnd, 0.8f);
            if (i == cur)
            {
                // ausgesparter Fleck wie in den Zellkreisen
                g.setColour (paper);
                g.fillEllipse (juce::Rectangle<float> (r.getWidth() * 0.34f, r.getHeight() * 0.42f).withCentre (r.getCentre().translated (0.0f, -3.0f)));
            }
            pencilText (g, juce::String (i + 1), r.removeFromBottom (15.0f), juce::Justification::centred, 10.0f, paper.withAlpha (0.8f));
        }
        else
        {
            auto p = roughRect (r, rnd, 1.6f);
            g.setColour (graphite.withAlpha (0.6f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
            g.drawLine (r.getX() + 4.0f, r.getBottom() - 4.0f, r.getRight() - 4.0f, r.getY() + 4.0f, 0.8f);
            pencilText (g, juce::String (i + 1), r.removeFromBottom (15.0f), juce::Justification::centred, 10.0f);
        }
    }
}

void StepRow::setParam (const juce::String& id, float plain)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

void StepRow::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, numChops - 1, (int) (e.position.x / ((float) getWidth() / numChops)));
    auto value = [this] (int c, const char* p) { return proc.apvts.getRawParameterValue (cid (c, p))->load(); };
    auto toggle = [this, value] (int c) { setParam (cid (c, "an"), value (c, "an") > 0.5f ? 0.0f : 1.0f); };
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m, paste;
        m.addSectionHeader ("Strich " + juce::String (i + 1));
        m.addItem (1, "Kopieren");
        paste.addItem (10, "alles", hasClip);
        const char* names[] = { "nur Position", "nur Korngr\xc3\xb6\xc3\x9f" "e", "nur Dichte", "nur Chaos", "nur Raum", "nur Tonh\xc3\xb6he" };
        for (int k = 0; k < numChopParams; ++k) paste.addItem (11 + k, juce::String::fromUTF8 (names[k]), hasClip);
        m.addSubMenu (juce::String::fromUTF8 ("Einf\xc3\xbcgen"), paste, hasClip);
        m.addItem (2, "Auf alle Striche kopieren");
        m.addSeparator();
        m.addItem (3, value (i, "an") > 0.5f ? "Strich aus" : "Strich an");
        m.addItem (4, juce::String::fromUTF8 ("Zuf\xc3\xa4llig (dieser Strich)"));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, i, value, toggle] (int id)
        {
            if (id == 1)
            {
                for (int k = 0; k < numChopParams; ++k) clip[k] = value (i, chopParams[k]);
                clip[numChopParams] = value (i, "an");
                hasClip = true;
            }
            else if (id == 10 && hasClip) { for (int k = 0; k < numChopParams; ++k) setParam (cid (i, chopParams[k]), clip[k]); setParam (cid (i, "an"), clip[numChopParams]); }
            else if (id >= 11 && id < 11 + numChopParams && hasClip) setParam (cid (i, chopParams[id - 11]), clip[id - 11]);
            else if (id == 2)
                for (int c = 0; c < numChops; ++c)
                    if (c != i) for (int k = 0; k < numChopParams; ++k) setParam (cid (c, chopParams[k]), value (i, chopParams[k]));
            else if (id == 3) toggle (i);
            else if (id == 4)
            {
                juce::Random r;
                for (int k = 0; k < numChopParams; ++k)
                    if (auto* p = proc.apvts.getParameter (cid (i, chopParams[k])))
                    {
                        const float v = k == 5 ? (float) (r.nextInt (25) - 12) : p->convertFrom0to1 (r.nextFloat());
                        setParam (cid (i, chopParams[k]), v);
                    }
            }
        });
        return;
    }
    if (e.mods.isShiftDown()) { toggle (i); return; }
    setParam ("auswahl", (float) (i + 1));
}

//==============================================================================
// ChopKnob
ChopKnob::ChopKnob (TuscheProcessor& p, int paramIndex, const juce::String& cap) : proc (p), k (paramIndex), caption (cap)
{
    slider.setRange (0.0, 1.0);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    auto* p0 = param (0);
    slider.setDoubleClickReturnValue (true, p0->getDefaultValue());
    slider.textFromValueFunction = [this] (double v) { return format ((float) v); };
    slider.valueFromTextFunction = [this] (const juce::String& t) { return (double) param (chop)->getValueForText (t); };
    slider.onDragStart = [this]
    {
        for (int c = 0; c < numChops; ++c) if (c == chop || linked()) param (c)->beginChangeGesture();
    };
    slider.onDragEnd = [this]
    {
        for (int c = 0; c < numChops; ++c) if (c == chop || linked()) param (c)->endChangeGesture();
    };
    slider.onValueChange = [this]
    {
        if (syncing) return;
        const float v = (float) slider.getValue();
        for (int c = 0; c < numChops; ++c) if (c == chop || linked()) param (c)->setValueNotifyingHost (v);
    };
    addAndMakeVisible (slider);
}

juce::String ChopKnob::format (float v) const
{
    const float x = param (chop)->convertFrom0to1 (v);
    switch (k)
    {
        case 1:  return juce::String (juce::roundToInt (x)) + " ms";
        case 2:
            if (proc.apvts.getRawParameterValue ("sync")->load() > 0.5f) return grainDivs[juce::roundToInt (x * 8.0f)];
            return juce::String (1.0f + x * x * 79.0f, 1) + " Hz";
        case 5:  return (x > 0 ? "+" : "") + juce::String (juce::roundToInt (x)) + " HT";
        default: return juce::String (juce::roundToInt (x * 100.0f)) + " %";
    }
}

bool ChopKnob::linked() const { return (bool) proc.apvts.state.getProperty ("link_" + juce::String (k), false); }

void ChopKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    r.removeFromBottom (17);
    slider.setBounds (r);
}

void ChopKnob::paint (juce::Graphics& g)
{
    using namespace ink;
    pencilText (g, caption.toUpperCase(), getLocalBounds().removeFromTop (16).toFloat(), juce::Justification::centred, 11.0f, ink::ink.withAlpha (0.75f));
    // Verknüpfung: „alle“ – grüner Strich, wenn aktiv
    auto a = linkArea().toFloat();
    juce::Random rnd (k + 50);
    if (linked())
    {
        juce::Path pth; pth.startNewSubPath (a.getX() + 2.0f, a.getCentreY() + 2.0f); pth.lineTo (a.getRight() - 2.0f, a.getCentreY());
        dryStroke (g, pth, 7.0f, green, rnd, 0.6f);
        pencilText (g, "ALLE", a, juce::Justification::centred, 9.5f, ink::ink);
    }
    else
        pencilText (g, "ALLE", a, juce::Justification::centred, 9.5f, graphite.withAlpha (0.55f));
}

void ChopKnob::mouseDown (const juce::MouseEvent& e)
{
    if (linkArea().contains (e.getPosition()))
    {
        proc.apvts.state.setProperty ("link_" + juce::String (k), ! linked(), nullptr);
        repaint();
    }
}

void ChopKnob::refresh (int c)
{
    chop = c;
    if (slider.isMouseButtonDown()) return;
    const juce::ScopedValueSetter<bool> s (syncing, true);
    slider.setValue (param (c)->getValue(), juce::dontSendNotification);
    slider.updateText();
}

//==============================================================================
void EnvView::paint (juce::Graphics& g)
{
    using namespace ink;
    auto r = getLocalBounds().toFloat().reduced (6.0f, 8.0f);
    const float a = proc.apvts.getRawParameterValue ("attack")->load(), d = proc.apvts.getRawParameterValue ("decay")->load();
    const float s = proc.apvts.getRawParameterValue ("sustain")->load(), rel = proc.apvts.getRawParameterValue ("release")->load();
    auto tx = [] (float ms) { return std::log1p (ms / 40.0f); };
    const float total = tx (a) + tx (d) + 1.2f + tx (rel);
    const float sx = r.getWidth() / total;
    float x = r.getX();
    juce::Path p;
    p.startNewSubPath (x, r.getBottom());
    x += tx (a) * sx; p.lineTo (x, r.getY());
    x += tx (d) * sx; p.lineTo (x, r.getBottom() - s * r.getHeight());
    x += 1.2f * sx;   p.lineTo (x, r.getBottom() - s * r.getHeight());
    x += tx (rel) * sx; p.lineTo (x, r.getBottom());
    juce::Random rnd (5);
    g.setColour (ink::ink.withAlpha (0.06f));
    juce::Path fill (p); fill.closeSubPath(); g.fillPath (fill);
    dryStroke (g, p, 4.0f, ink::ink, rnd, 0.5f);
}

//==============================================================================
static tmk::Theme inkTheme()
{
    tmk::Theme t;
    t.accent  = ink::green;
    t.text    = ink::ink;
    t.dim     = juce::Colour (0xff3e3e3e);
    t.glass   = juce::Colour (0x00000000);
    t.outline = juce::Colour (0x40000000);
    t.bg      = ink::paper;
    t.knobTop = ink::paper;
    t.knobBot = ink::paper;
    return t;
}

tmk::Editor::Config TuscheEditor::makeConfig (TuscheProcessor& p)
{
    Config c;
    c.title = "TUSCHE";
    c.subtitle = "granular-sampler";
    c.textureName = "tusche";
    c.texture = [] { return loadAsset (TuscheAssets::papier_jpg, TuscheAssets::papier_jpgSize); };
    c.theme = inkTheme();
    c.width = 1280; c.height = 930;
    c.keyboardState = &p.keyboardState;
    return c;
}

TuscheEditor::TuscheEditor (TuscheProcessor& p)
    : tmk::Editor (p, makeConfig (p)), tp (p), inkLnf (cfg.theme), display (p), steps (p), env (p)
{
    setLookAndFeel (&inkLnf);
    if (auto* kb = getKeyboard())
    {
        kb->setColour (juce::MidiKeyboardComponent::whiteNoteColourId, ink::paper);
        kb->setColour (juce::MidiKeyboardComponent::blackNoteColourId, ink::ink);
        kb->setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, ink::graphite.withAlpha (0.5f));
        kb->setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ink::green.withAlpha (0.7f));
        kb->setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ink::green.withAlpha (0.25f));
        kb->setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colour (0x18000000));
    }

    display.onPositionDrag = [this] (float pos)
    {
        const int sel = juce::jlimit (0, numChops - 1, (int) tp.apvts.getRawParameterValue ("auswahl")->load() - 1);
        tp.apvts.getParameter (cid (sel, "pos"))->setValueNotifyingHost (pos);
    };
    display.onGesture = [this] (bool begin)
    {
        const int sel = juce::jlimit (0, numChops - 1, (int) tp.apvts.getRawParameterValue ("auswahl")->load() - 1);
        auto* prm = tp.apvts.getParameter (cid (sel, "pos"));
        if (begin) prm->beginChangeGesture(); else prm->endChangeGesture();
    };

    const char* caps[] = { "Position", "Korn", "Dichte", "Chaos", "Raum", "Ton" };
    std::vector<Item> chopItems;
    for (int k = 0; k < numChopParams; ++k)
    {
        chopKnobs.push_back (std::make_unique<ChopKnob> (p, k, juce::String::fromUTF8 (k == 5 ? "TONH\xc3\x96HE" : caps[k])));
        chopItems.push_back (custom (*chopKnobs.back(), 1.0f));
    }

    addSection ("BLATT", { custom (display, 1.0f) });
    addSection ("SPIEL", { combo ("modus", "Modus", 1.2f), toggle ("sync", "Sync", 0.8f), combo ("schritt", u8 ("Schritt"), 1.0f),
                           combo ("quelle", "Quelle", 1.5f), knob ("wandern", "Wandern") });
    addSection (u8 ("H\xc3\x9cLLE"), { custom (env, 1.6f), knob ("attack", "Attack"), knob ("decay", "Decay"), knob ("sustain", "Sustain"),
                                      knob ("release", "Release") });
    addSection ("PARTITUR", { toggle ("partitur", "Blatt lesen", 1.2f), combo ("takte", juce::String::fromUTF8 ("L\xc3\xa4nge"), 1.2f),
                              knob ("einfluss", "Einfluss") });
    addSection ("STRICHE", { custom (steps, 1.0f) });
    addSection ("STRICH", chopItems);
    addSection ("KLANG", { knob ("tief", "Tiefpass"), knob ("hoch", "Hochpass"), knob ("reso", "Resonanz"), knob ("raumgr", "Raum"),
                           knob ("ausgang", "Ausgang") });
    finishSetup();
    std::function<void (juce::Component&)> pencil = [&] (juce::Component& comp)
    {
        for (auto* ch : comp.getChildren())
        {
            if (auto* l = dynamic_cast<juce::Label*> (ch); l != nullptr && dynamic_cast<juce::Slider*> (&comp) == nullptr
                && dynamic_cast<juce::ComboBox*> (&comp) == nullptr)
            {
                l->setFont (juce::FontOptions (12.0f).withStyle ("Italic"));
                l->setColour (juce::Label::textColourId, ink::ink.withAlpha (0.72f));
            }
            if (auto* sl = dynamic_cast<juce::Slider*> (ch))
            {
                sl->setColour (juce::Slider::textBoxTextColourId, ink::ink);
                sl->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
                sl->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            }
            pencil (*ch);
        }
    };
    pencil (*this);
    tick();
}

TuscheEditor::~TuscheEditor() { setLookAndFeel (nullptr); }

void TuscheEditor::tick()
{
    const int sel = juce::jlimit (0, numChops - 1, (int) tp.apvts.getRawParameterValue ("auswahl")->load() - 1);
    for (auto& k : chopKnobs) k->refresh (sel);
    if (sel != shownChop) { shownChop = sel; repaint(); }
}

void TuscheEditor::layoutBody (juce::Rectangle<int> body)
{
    auto top = body.removeFromTop (body.getHeight() * 53 / 100);
    layoutSection ("BLATT", top.removeFromLeft (top.getWidth() * 60 / 100));
    const int h3 = top.getHeight() / 3;
    layoutSection ("SPIEL", top.removeFromTop (h3));
    layoutSection ("PARTITUR", top.removeFromTop (h3));
    layoutSection (juce::String::fromUTF8 ("H\xc3\x9cLLE"), top);
    layoutSection ("STRICHE", body.removeFromTop (96));
    auto bottom = split (body, { 6, 5 });
    layoutSection ("STRICH", bottom[0]);
    layoutSection ("KLANG", bottom[1]);
}

void TuscheEditor::paintHeader (juce::Graphics& g, juce::Rectangle<float>)
{
    using namespace ink;
    juce::Random rnd (2024);
    // Titel als Druckblock mit ausgesparter Schrift
    auto tb = juce::Rectangle<float> (16.0f, 12.0f, 150.0f, 40.0f);
    stamp (g, tb, ink::ink, rnd, 0.7f);
    g.setColour (paper);
    g.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    g.drawText ("TUSCHE", tb, juce::Justification::centred);
    pencilText (g, "granular-sampler", { 178.0f, 14.0f, 160.0f, 16.0f }, juce::Justification::centredLeft, 11.0f, ink::ink.withAlpha (0.7f));
    pencilText (g, "trockenmoos audio", { 178.0f, 31.0f, 160.0f, 16.0f }, juce::Justification::centredLeft, 11.0f);
    pencilText (g, versionText(), { 340.0f, 31.0f, 80.0f, 16.0f },
                juce::Justification::centredLeft, 11.0f, ink::green.darker (0.2f));
    // Signatur
    pencilText (g, "CC", { (float) cfg.width - 50.0f, (float) cfg.height - 22.0f, 36.0f, 16.0f }, juce::Justification::centredRight, 12.0f);
}

bool TuscheEditor::paintSection (juce::Graphics& g, juce::Rectangle<float> b, const juce::String& title)
{
    using namespace ink;
    if (title == "BLATT") return true;                 // die Anzeige ist ihr eigener Rahmen
    juce::Random rnd (title.hashCode());
    auto p = roughRect (b, rnd, 1.4f);
    g.setColour (ink::ink.withAlpha (0.025f));
    g.fillPath (p);
    g.setColour (graphite.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (0.9f));
    juce::String t = title;
    if (title == "STRICH")
        t = "STRICH " + juce::String ((int) tp.apvts.getRawParameterValue ("auswahl")->load());
    pencilText (g, t, b.reduced (12.0f, 6.0f).withHeight (16.0f), juce::Justification::centredLeft, 11.5f, ink::ink.withAlpha (0.8f));
    if (title == "STRICHE")
        pencilText (g, juce::String::fromUTF8 ("Klick w\xc3\xa4hlt \xc2\xb7 Umschalt+Klick an/aus \xc2\xb7 Rechtsklick: kopieren, einf\xc3\xbcgen, zuf\xc3\xa4llig"),
                    b.reduced (12.0f, 6.0f).withHeight (16.0f), juce::Justification::centredRight, 9.5f);
    return true;
}

bool TuscheEditor::paintKeyboardFrame (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Random rnd (9);
    g.setColour (ink::graphite.withAlpha (0.5f));
    g.strokePath (ink::roughRect (r.expanded (3.0f), rnd, 1.2f), juce::PathStrokeType (1.0f));
    return true;
}

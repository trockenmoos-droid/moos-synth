#include "PluginEditor.h"

using namespace MoosColours;

//==============================================================================
MoosLookAndFeel::MoosLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, lichen);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x50000000));
    setColour (juce::Label::textColourId, lichen);
    setColour (juce::ComboBox::textColourId, lichen);
    setColour (juce::ComboBox::arrowColourId, moss);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf2141b10));
    setColour (juce::PopupMenu::textColourId, lichen);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, moss.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff10160c));
    setColour (juce::PopupMenu::headerTextColourId, moss);
    setColour (juce::TextButton::textColourOffId, lichen);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff10160c));
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1a2214));
    setColour (juce::TextEditor::textColourId, lichen);
}

void MoosLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds  = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centre  = bounds.getCentre();
    auto angle   = startAngle + pos * (endAngle - startAngle);
    const float lw = 3.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - lw, radius - lw, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0x50000000));
    g.strokePath (track, juce::PathStrokeType (lw + 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolare Regler (z. B. -1..+1) füllen von der Mitte aus
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - lw, radius - lw, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (moss);
    g.strokePath (value, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knobArea = juce::Rectangle<float> (radius * 1.25f, radius * 1.25f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4630), knobArea.getX(), knobArea.getY(),
                                             juce::Colour (0xff141a10), knobArea.getRight(), knobArea.getBottom(), false));
    g.fillEllipse (knobArea);
    g.setColour (juce::Colour (0x30ffffff));
    g.drawEllipse (knobArea, 1.0f);

    juce::Point<float> tip (centre.x + (radius * 0.55f) * std::sin (angle),
                            centre.y - (radius * 0.55f) * std::cos (angle));
    g.setColour (lichen);
    g.drawLine ({ centre.getPointOnCircumference (radius * 0.12f, angle), tip }, 2.0f);
}

void MoosLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        juce::Slider::SliderStyle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (juce::Colour (0x60000000));
    g.fillRoundedRectangle (r, 3.0f);
    const float mid = s.getMinimum() < 0.0 ? r.getCentreX() : r.getX();
    g.setColour (moss);
    g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (mid, pos), r.getY(), juce::jmax (mid, pos), r.getBottom()), 3.0f);
    g.setColour (lichen);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pos, r.getCentreY() }));
}

void MoosLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? moss : juce::Colour (over ? 0x70000000 : 0x50000000));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? moss.brighter (0.3f) : outline.withAlpha (down ? 0.9f : 0.6f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void MoosLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (juce::Colour (0x60000000));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (outline.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (moss);
    g.fillPath (arrow);
}

//==============================================================================
WavetableView::WavetableView (MoosProcessor& p, int oscIndex) : proc (p), osc (oscIndex)
{
    startTimerHz (30);
    setInterceptsMouseClicks (false, false);
}

void WavetableView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0x70000000));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const auto* bank = proc.getBank (osc);
    if (bank == nullptr) return;

    const float base   = proc.apvts.getRawParameterValue (osc == 0 ? "posA" : "posB")->load();
    const float lfoAmt = proc.apvts.getRawParameterValue ("lfoPos")->load();
    const float pos    = juce::jlimit (0.0f, 1.0f, base + 0.5f * lfoAmt * proc.getLfoValue (0));
    const int   nf     = bank->getNumFrames();
    const float fp     = pos * (float) (nf - 1);
    const int   f0     = juce::jlimit (0, nf - 1, (int) fp);
    const int   f1     = juce::jmin (f0 + 1, nf - 1);
    const float ff     = fp - (float) f0;

    auto area = r.reduced (10.0f, 18.0f).withTrimmedTop (6.0f);
    g.setColour (outline.withAlpha (0.3f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    const float* t0 = bank->get (0, f0);
    const float* t1 = bank->get (0, f1);
    juce::Path wave;
    for (int i = 0; i <= 256; ++i)
    {
        const int idx = juce::jmin (i * 8, WavetableBank::tableSize);
        const float v = t0[idx] + ff * (t1[idx] - t0[idx]);
        const float xx = area.getX() + area.getWidth() * (float) i / 256.0f;
        const float yy = area.getCentreY() - v * area.getHeight() * 0.45f;
        i == 0 ? wave.startNewSubPath (xx, yy) : wave.lineTo (xx, yy);
    }
    g.setColour (moss.withAlpha (0.25f));
    g.strokePath (wave, juce::PathStrokeType (6.0f));
    g.setColour (moss);
    g.strokePath (wave, juce::PathStrokeType (2.0f));

    g.setColour (lichen);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    juce::String name = bank->frameName (f0);
    if (ff > 0.05f && f1 != f0) name << " > " << bank->frameName (f1);
    g.drawText (name, r.reduced (8.0f, 4.0f), juce::Justification::topLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (bank->getName() + "  (" + juce::String (nf) + ")", r.reduced (8.0f, 4.0f), juce::Justification::bottomRight);
}

//==============================================================================
ModMatrixComponent::ModMatrixComponent (MoosProcessor& p)
{
    for (int i = 0; i < MoosParams::numModSlots; ++i)
    {
        auto& r = rows[i];
        const auto n = juce::String (i + 1);
        r.src.addItemList (MoosParams::modSources, 1);
        r.dst.addItemList (MoosParams::modDests, 1);
        r.amount.setSliderStyle (juce::Slider::LinearHorizontal);
        r.amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
        addAndMakeVisible (r.src);
        addAndMakeVisible (r.dst);
        addAndMakeVisible (r.amount);
        r.srcAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "mod" + n + "Src", r.src);
        r.dstAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "mod" + n + "Dst", r.dst);
        r.amtAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "mod" + n + "Amt", r.amount);
        r.amount.textFromValueFunction = [] (double v) { return juce::String (v, 2); };
        r.amount.updateText();
    }
}

void ModMatrixComponent::resized()
{
    auto area = getLocalBounds().withTrimmedLeft (34).withTrimmedTop (24);
    const int rowH = area.getHeight() / MoosParams::numModSlots;
    for (auto& r : rows)
    {
        auto row = area.removeFromTop (rowH).withSizeKeepingCentre (area.getWidth(), 30);
        r.src.setBounds (row.removeFromLeft (190));
        row.removeFromLeft (40);
        r.dst.setBounds (row.removeFromLeft (190));
        row.removeFromLeft (24);
        r.amount.setBounds (row);
    }
}

void ModMatrixComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().withTrimmedTop (24);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (dim);
    g.drawText ("QUELLE", 34, 2, 190, 18, juce::Justification::centredLeft);
    g.drawText ("ZIEL", 264, 2, 190, 18, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("ST\xc3\x84RKE"), 478, 2, 190, 18, juce::Justification::centredLeft);

    const int rowH = area.getHeight() / MoosParams::numModSlots;
    for (int i = 0; i < MoosParams::numModSlots; ++i)
    {
        auto row = area.removeFromTop (rowH);
        g.setColour (moss);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (juce::String (i + 1), row.removeFromLeft (28), juce::Justification::centred);
        g.setColour (dim);
        g.drawText (">", juce::Rectangle<int> (224, row.getY(), 40, row.getHeight()), juce::Justification::centred);
    }
}

//==============================================================================
MoosEditor::MoosEditor (MoosProcessor& p)
    : AudioProcessorEditor (&p), proc (p), viewA (p, 0), viewB (p, 1), matrix (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.onPaint = [this] (juce::Graphics& g) { paintContent (g); };

    loadBackground();

    // --- SEITE 1: KLANG ---
    addSection ("OSC A", 0, { custom (viewA, 2.6f), knob ("posA", "Position"), knob ("octA", "Octave"), knob ("levelA", "Level") })
        .headerButtons = { &loadA, &resetA };
    addSection ("OSC B", 0, { custom (viewB, 2.6f), knob ("posB", "Position"), knob ("octB", "Octave"),
                              knob ("fineB", "Fine"), knob ("levelB", "Level") })
        .headerButtons = { &loadB, &resetB };
    addSection ("UNISON", 0, { knob ("uniVoices", "Voices"), knob ("uniDetune", "Detune"), knob ("uniWidth", "Width"), knob ("drift", "Drift") });
    addSection ("FILTER", 0, { knob ("hpf", "Low Cut"), knob ("cutoff", "Cutoff"), knob ("reso", "Reso"), knob ("envAmt", "Env Amt") });
    addSection ("FILTER ENV", 0, { knob ("fA", "Attack"), knob ("fD", "Decay"), knob ("fS", "Sustain"), knob ("fR", "Release") });
    addSection ("AMP ENV", 0, { knob ("aA", "Attack"), knob ("aD", "Decay"), knob ("aS", "Sustain"), knob ("aR", "Release") });
    addSection ("SPIELWEISE", 0, { knob ("glide", "Glide"), knob ("bendRange", "Bend"), toggle ("mono", "MONO") });
    addSection ("AUSGANG", 0, { knob ("volume", "Volume") });

    // --- SEITE 2: MODULATION ---
    addSection ("LFO 1", 1, { knob ("lfoRate", "Rate"), combo ("lfoShape", "Form"), knob ("lfoPos", "> Position"), knob ("lfoCut", "> Cutoff") });
    addSection ("LFO 2", 1, { knob ("lfo2Rate", "Rate"), combo ("lfo2Shape", "Form") });
    matrixHint.setText (juce::String::fromUTF8 ("Mod Wheel = CC1  /  Aftertouch = Kanal- oder polyphoner Druck  /  "
                        "Note Random = Zufallswert pro Note  /  Key Track = Tonh\xc3\xb6he"), juce::dontSendNotification);
    matrixHint.setColour (juce::Label::textColourId, dim);
    matrixHint.setFont (juce::FontOptions (12.0f));
    matrixHint.setJustificationType (juce::Justification::centredLeft);
    content.addAndMakeVisible (matrixHint);
    addSection ("MOD MATRIX", 1, { custom (matrix, 1.0f) });

    // --- SEITE 3: EFFEKTE & ARP ---
    addSection ("CHORUS", 2, { knob ("chorusMix", "Mix") });
    addSection ("DELAY", 2, { toggle ("delaySync", "SYNC"), combo ("delayDiv", "Takt"), knob ("delayTime", "Zeit ms"),
                              knob ("delayFb", "Feedback"), knob ("delayMix", "Mix") });
    addSection ("HALL", 2, { knob ("reverbSize", juce::String::fromUTF8 ("Gr\xc3\xb6\xc3\x9f" "e")), knob ("reverbMix", "Mix"), knob ("shimmer", "Shimmer") });
    addSection ("ARPEGGIATOR", 2, { toggle ("arpOn", "AN"), combo ("arpMode", "Modus"), combo ("arpRate", "Rate"),
                                    knob ("arpOct", "Oktaven"), knob ("arpGate", "Gate") });

    for (auto* b : { &loadA, &resetA, &loadB, &resetB }) content.addAndMakeVisible (*b);
    loadA.onClick = [this] { chooseWavetable (0); };
    loadB.onClick = [this] { chooseWavetable (1); };
    resetA.onClick = [this] { proc.resetWavetable (0); };
    resetB.onClick = [this] { proc.resetWavetable (1); };
    loadA.setTooltip ("Eigene Wavetable / Sample (WAV, AIFF, FLAC) laden");

    // --- Kopfzeile ---
    const char* pageNames[] = { "KLANG", "MODULATION", "FX / ARP" };
    for (int i = 0; i < 3; ++i)
    {
        pageButtons[i].setButtonText (pageNames[i]);
        pageButtons[i].setClickingTogglesState (false);
        pageButtons[i].onClick = [this, i] { showPage (i); };
        content.addAndMakeVisible (pageButtons[i]);
    }

    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this] { presetSelected(); };
    content.addAndMakeVisible (presetBox);
    for (auto* b : { &prevPreset, &nextPreset, &savePresetButton, &bgButton }) content.addAndMakeVisible (*b);
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    savePresetButton.onClick = [this] { savePreset(); };
    bgButton.onClick = [this] { backgroundMenu(); };

    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, moss);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, moss.withAlpha (0.4f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colour (0x40000000));
    keyboard.setLowestVisibleKey (36);
    content.addAndMakeVisible (keyboard);

    refreshPresetList();
    showPage (0);
    layoutContent();

    setResizable (true, true);
    setResizeLimits (840, 546, 2400, 1560);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (baseW, baseH);
    startTimerHz (5);
}

MoosEditor::~MoosEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
juce::Label* MoosEditor::makeLabel (const juce::String& text)
{
    auto l = std::make_unique<juce::Label> (juce::String(), text);
    l->setJustificationType (juce::Justification::centred);
    l->setFont (juce::FontOptions (12.0f));
    l->setColour (juce::Label::textColourId, dim.brighter (0.3f));
    l->setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (*l);
    labels.push_back (std::move (l));
    return labels.back().get();
}

MoosEditor::Item MoosEditor::knob (const juce::String& id, const juce::String& text)
{
    auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 17);
    content.addAndMakeVisible (*s);
    sliderAtts.push_back (std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, *s));
    s->textFromValueFunction = [sl = s.get()] (double v)
    {
        if (sl->getInterval() >= 1.0) return juce::String (juce::roundToInt (v));
        const double a = std::abs (v);
        return juce::String (v, a >= 100.0 ? 0 : (a >= 10.0 ? 1 : 2));
    };
    s->updateText();
    Item it { s.get(), makeLabel (text), 1.0f, 0 };
    owned.push_back (std::move (s));
    return it;
}

MoosEditor::Item MoosEditor::combo (const juce::String& id, const juce::String& text)
{
    auto c = std::make_unique<juce::ComboBox>();
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->addItemList (p->choices, 1);
    content.addAndMakeVisible (*c);
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, *c));
    Item it { c.get(), makeLabel (text), 1.3f, 1 };
    owned.push_back (std::move (c));
    return it;
}

MoosEditor::Item MoosEditor::toggle (const juce::String& id, const juce::String& text)
{
    auto b = std::make_unique<juce::TextButton> (text);
    b->setClickingTogglesState (true);
    content.addAndMakeVisible (*b);
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, *b));
    Item it { b.get(), makeLabel (" "), 0.9f, 1 };
    owned.push_back (std::move (b));
    return it;
}

MoosEditor::Item MoosEditor::custom (juce::Component& c, float weight)
{
    content.addAndMakeVisible (c);
    return { &c, nullptr, weight, 2 };
}

MoosEditor::Section& MoosEditor::addSection (const juce::String& title, int page, std::vector<Item> items)
{
    sections.push_back ({ title, page, std::move (items), {}, {} });
    return sections.back();
}

//==============================================================================
void MoosEditor::showPage (int page)
{
    currentPage = page;
    for (int i = 0; i < 3; ++i) pageButtons[i].setToggleState (i == page, juce::dontSendNotification);
    for (auto& s : sections)
    {
        const bool vis = s.page == page;
        for (auto& it : s.items)
        {
            it.comp->setVisible (vis);
            if (it.label != nullptr) it.label->setVisible (vis);
        }
        for (auto* b : s.headerButtons) b->setVisible (vis);
    }
    matrixHint.setVisible (page == 1);
    content.repaint();
}

void MoosEditor::layoutSection (Section& s, juce::Rectangle<int> area)
{
    s.bounds = area;
    auto header = area.reduced (8).removeFromTop (20);
    for (int i = (int) s.headerButtons.size(); --i >= 0;)
    {
        s.headerButtons[(size_t) i]->setBounds (header.removeFromRight (i == (int) s.headerButtons.size() - 1 ? 26 : 46).reduced (1, 0));
        header.removeFromRight (4);
    }

    auto inner = area.reduced (10, 8).withTrimmedTop (22);
    float total = 0.0f;
    for (auto& it : s.items) total += it.weight;

    const int fullW = inner.getWidth();
    for (auto& it : s.items)
    {
        auto cell = inner.removeFromLeft ((int) std::round (fullW * it.weight / total));
        if (it.kind == 2) { it.comp->setBounds (cell.reduced (4, 2)); continue; }

        if (it.label != nullptr) it.label->setBounds (cell.removeFromTop (16));
        if (it.kind == 0)
        {
            it.comp->setBounds (cell.withSizeKeepingCentre (juce::jlimit (60, 92, cell.getWidth() - 4), juce::jmin (cell.getHeight(), 112)));
        }
        else
            it.comp->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 10, 28).translated (0, -10));
    }
}

void MoosEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (juce::AffineTransform::scale (scale));
}

void MoosEditor::layoutContent()
{
    auto r = juce::Rectangle<int> (0, 0, baseW, baseH).reduced (12, 8);

    // Kopfzeile
    auto head = r.removeFromTop (48);
    head.removeFromLeft (190);                         // Logo
    for (auto& b : pageButtons) { b.setBounds (head.removeFromLeft (110).withSizeKeepingCentre (104, 30)); }
    head.removeFromLeft (20);
    bgButton.setBounds (head.removeFromRight (108).withSizeKeepingCentre (104, 30));
    head.removeFromRight (8);
    savePresetButton.setBounds (head.removeFromRight (98).withSizeKeepingCentre (94, 30));
    head.removeFromRight (8);
    nextPreset.setBounds (head.removeFromRight (32).withSizeKeepingCentre (30, 30));
    presetBox.setBounds (head.removeFromRight (230).withSizeKeepingCentre (226, 30));
    prevPreset.setBounds (head.removeFromRight (32).withSizeKeepingCentre (30, 30));

    keyboard.setBounds (r.removeFromBottom (78));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setLowestVisibleKey (36);
    r.removeFromBottom (6);

    const int rowH = r.getHeight() / 3;
    auto row = [&] (int i) { return juce::Rectangle<int> (r.getX(), r.getY() + i * rowH, r.getWidth(), rowH); };
    auto split = [] (juce::Rectangle<int> a, std::initializer_list<int> w)
    {
        std::vector<juce::Rectangle<int>> out;
        int total = 0; for (int x : w) total += x;
        const int fullW = a.getWidth();
        for (int x : w) out.push_back (a.removeFromLeft (fullW * x / total));
        return out;
    };
    auto find = [this] (const juce::String& t) -> Section& { for (auto& s : sections) if (s.title == t) return s; return sections[0]; };

    // Seite 1
    auto r1 = split (row (0), { 54, 66 });
    layoutSection (find ("OSC A"), r1[0]);
    layoutSection (find ("OSC B"), r1[1]);
    auto r2 = split (row (1), { 4, 4, 4 });
    layoutSection (find ("UNISON"), r2[0]);
    layoutSection (find ("FILTER"), r2[1]);
    layoutSection (find ("FILTER ENV"), r2[2]);
    auto r3 = split (row (2), { 4, 3, 2 });
    layoutSection (find ("AMP ENV"), r3[0]);
    layoutSection (find ("SPIELWEISE"), r3[1]);
    layoutSection (find ("AUSGANG"), r3[2]);

    // Seite 2
    auto m1 = split (row (0), { 3, 2 });
    layoutSection (find ("LFO 1"), m1[0]);
    layoutSection (find ("LFO 2"), m1[1]);
    auto matrixArea = row (1).getUnion (row (2));
    matrixHint.setBounds (matrixArea.removeFromBottom (30).reduced (16, 0));
    layoutSection (find ("MOD MATRIX"), matrixArea);

    // Seite 3
    auto f1 = split (row (0), { 1, 5 });
    layoutSection (find ("CHORUS"), f1[0]);
    layoutSection (find ("DELAY"), f1[1]);
    auto f2 = split (row (1), { 3, 3 });
    layoutSection (find ("HALL"), f2[0]);
    layoutSection (find ("ARPEGGIATOR"), row (2));
    juce::ignoreUnused (f2);
}

//==============================================================================
void MoosEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
}

void MoosEditor::paintContent (juce::Graphics& g)
{
    const auto full = juce::Rectangle<float> (0, 0, (float) baseW, (float) baseH);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (bgImage.isValid()) g.drawImage (bgImage, full, juce::RectanglePlacement::fillDestination);
    else                   g.fillAll (bg);

    // Kopfzeile
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xc0050804), 0, 0, juce::Colour (0x00050804), 0, 70, false));
    g.fillRect (full.withHeight (70));

    g.setColour (moss);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("MOOS", 18, 10, 120, 40, juce::Justification::centredLeft);
    g.setColour (lichen.withAlpha (0.7f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("wavetable pads", 108, 14, 90, 16, juce::Justification::centredLeft);
    g.drawText ("trockenmoos  v0.2", 108, 28, 90, 16, juce::Justification::centredLeft);

    for (auto& s : sections)
    {
        if (s.page != currentPage) continue;
        auto b = s.bounds.toFloat().reduced (4.0f);

        // "Milchglas": weichgezeichneter Hintergrund + dunkle Tönung
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (b, 8.0f);
            g.reduceClipRegion (clip);
            if (bgBlur.isValid()) g.drawImage (bgBlur, full, juce::RectanglePlacement::fillDestination);
            g.setColour (glass);
            g.fillRect (b);
        }
        g.setColour (outline);
        g.drawRoundedRectangle (b, 8.0f, 1.0f);

        g.setColour (moss);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (s.title, b.reduced (12.0f, 7.0f).toNearestInt(), juce::Justification::topLeft);
    }

    if (currentPage == 1)
    {
        auto r = matrixHint.getBounds().toFloat().expanded (6.0f, 2.0f);
        g.setColour (juce::Colour (0x90000000));
        g.fillRoundedRectangle (r, 6.0f);
    }

    // Hintergrund hinter der Tastatur
    g.setColour (juce::Colour (0xa0000000));
    g.fillRoundedRectangle (keyboard.getBounds().toFloat().expanded (3.0f), 4.0f);
}

//==============================================================================
void MoosEditor::timerCallback()
{
    if (waitingForMoss && mossCache->get (bgImage, bgBlur))
    {
        waitingForMoss = false;
        content.repaint();
    }

    if (proc.presetChangeCounter.load() != lastPresetCounter)
    {
        lastPresetCounter = proc.presetChangeCounter.load();
        refreshPresetList();
        content.repaint();
    }
}

void MoosEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Werkspresets");
    const auto& f = MoosPresets::factory();
    for (int i = 0; i < (int) f.size(); ++i)
        presetBox.addItem (f[(size_t) i].name, i + 1);

    userPresetFiles = MoosPresets::userPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("Eigene Presets");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), 1000 + i);
    }

    // aktuellen Namen anzeigen
    const auto name = proc.getPresetName();
    int id = 0;
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        if (presetBox.getItemText (i) == name) id = presetBox.getItemId (i);
    if (id != 0) presetBox.setSelectedId (id, juce::dontSendNotification);
    else         presetBox.setText (name, juce::dontSendNotification);
}

void MoosEditor::presetSelected()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0) return;
    if (id < 1000) proc.loadFactoryPreset (id - 1);
    else if (juce::isPositiveAndBelow (id - 1000, userPresetFiles.size()))
    {
        const auto err = proc.loadUserPreset (userPresetFiles[id - 1000]);
        if (err.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", err);
    }
}

void MoosEditor::stepPreset (int delta)
{
    const int n = presetBox.getNumItems();
    if (n == 0) return;
    int idx = 0;
    for (int i = 0; i < n; ++i) if (presetBox.getItemId (i) == presetBox.getSelectedId()) idx = i;
    idx = (idx + delta + n) % n;
    presetBox.setSelectedId (presetBox.getItemId (idx), juce::sendNotificationSync);
}

void MoosEditor::savePreset()
{
    auto folder = MoosPresets::userFolder();
    chooser = std::make_unique<juce::FileChooser> ("Preset speichern",
                                                   folder.getChildFile (proc.getPresetName() + ".moospreset"),
                                                   "*.moospreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension ("moospreset");
                              if (! proc.saveUserPreset (f))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Preset", "Speichern fehlgeschlagen.");
                          });
}

void MoosEditor::chooseWavetable (int osc)
{
    chooser = std::make_unique<juce::FileChooser> ("Wavetable oder Sample laden (WAV, AIFF, FLAC)",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, osc] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (! f.existsAsFile()) return;
                              const auto err = proc.loadWavetable (osc, f);
                              if (err.isNotEmpty())
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Wavetable", err);
                          });
}

void MoosEditor::backgroundMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Eigenes Foto laden ...");
    m.addItem (2, "Moos-Textur (Standard)", true, MossBackground::getPhotoPath().isEmpty());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (bgButton), [this] (int r)
    {
        if (r == 2) { MossBackground::setPhotoPath ({}); loadBackground(); }
        if (r == 1)
        {
            chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Hintergrundfoto w\xc3\xa4hlen"),
                                                           juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                                                           "*.jpg;*.jpeg;*.png");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (! f.existsAsFile()) return;
                                      MossBackground::setPhotoPath (f.getFullPathName());
                                      loadBackground();
                                  });
        }
    });
}

void MoosEditor::loadBackground()
{
    waitingForMoss = false;
    const auto path = MossBackground::getPhotoPath();
    if (path.isNotEmpty())
    {
        auto img = MossBackground::fromPhoto (juce::File (path));
        if (img.isValid())
        {
            bgImage = img;
            bgBlur  = MossBackground::blurred (img);
            content.repaint();
            return;
        }
    }
    if (! mossCache->get (bgImage, bgBlur))
    {
        bgImage = {}; bgBlur = {};
        waitingForMoss = true;      // Timer holt die Textur, sobald sie fertig ist
    }
    content.repaint();
}

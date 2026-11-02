#include "PluginEditor.h"

using namespace MoosColours;

//==============================================================================
MoosLookAndFeel::MoosLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, lichen);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, lichen);
}

void MoosLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    auto bounds  = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centre  = bounds.getCentre();
    auto angle   = startAngle + pos * (endAngle - startAngle);
    const float lw = 3.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - lw, radius - lw, 0.0f, startAngle, endAngle, true);
    g.setColour (outline);
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - lw, radius - lw, 0.0f, startAngle, angle, true);
    g.setColour (moss);
    g.strokePath (value, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (panel.brighter (0.15f));
    g.fillEllipse (juce::Rectangle<float> (radius * 1.2f, radius * 1.2f).withCentre (centre));

    juce::Point<float> tip (centre.x + (radius * 0.55f) * std::sin (angle),
                            centre.y - (radius * 0.55f) * std::cos (angle));
    g.setColour (lichen);
    g.drawLine ({ centre, tip }, 2.0f);
}

//==============================================================================
WavetableView::WavetableView (MoosProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void WavetableView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const auto& bank = proc.getBank();
    const float pos = proc.apvts.getRawParameterValue ("posA")->load();
    const float fp  = pos * (WavetableBank::numFrames - 1);
    const int   f0  = juce::jmin ((int) fp, WavetableBank::numFrames - 1);
    const int   f1  = juce::jmin (f0 + 1, WavetableBank::numFrames - 1);
    const float ff  = fp - (float) f0;

    auto area = r.reduced (10.0f, 16.0f);

    g.setColour (outline);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    const float* t0 = bank.get (0, f0);
    const float* t1 = bank.get (0, f1);
    juce::Path wave;
    for (int i = 0; i <= 256; ++i)
    {
        const int idx = juce::jmin (i * 8, WavetableBank::tableSize);
        const float v = t0[idx] + ff * (t1[idx] - t0[idx]);
        const float xx = area.getX() + area.getWidth() * (float) i / 256.0f;
        const float yy = area.getCentreY() - v * area.getHeight() * 0.45f;
        i == 0 ? wave.startNewSubPath (xx, yy) : wave.lineTo (xx, yy);
    }
    g.setColour (moss);
    g.strokePath (wave, juce::PathStrokeType (2.0f));

    g.setColour (lichen);
    g.setFont (juce::FontOptions (12.0f));
    juce::String name = WavetableBank::frameName (f0);
    if (ff > 0.05f && f1 != f0) name << " > " << WavetableBank::frameName (f1);
    g.drawText ("OSC A  " + name, r.reduced (8.0f, 4.0f), juce::Justification::topLeft);
}

//==============================================================================
MoosEditor::MoosEditor (MoosProcessor& p)
    : AudioProcessorEditor (&p), proc (p), tableView (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    sections = {
        { "OSC A",      { addKnob ("posA", "Position"), addKnob ("octA", "Octave"), addKnob ("levelA", "Level") }, {} },
        { "OSC B",      { addKnob ("posB", "Position"), addKnob ("octB", "Octave"), addKnob ("fineB", "Fine"), addKnob ("levelB", "Level") }, {} },
        { "UNISON",     { addKnob ("uniVoices", "Voices"), addKnob ("uniDetune", "Detune"), addKnob ("uniWidth", "Width") }, {} },
        { "FILTER",     { addKnob ("cutoff", "Cutoff"), addKnob ("reso", "Reso"), addKnob ("envAmt", "Env Amt") }, {} },
        { "FILTER ENV", { addKnob ("fA", "A"), addKnob ("fD", "D"), addKnob ("fS", "S"), addKnob ("fR", "R") }, {} },
        { "AMP ENV",    { addKnob ("aA", "A"), addKnob ("aD", "D"), addKnob ("aS", "S"), addKnob ("aR", "R") }, {} },
        { "LFO",        { addKnob ("lfoRate", "Rate"), addKnob ("lfoPos", "> Pos"), addKnob ("lfoCut", "> Cutoff") }, {} },
        { "FX",         { addKnob ("chorusMix", "Chorus"), addKnob ("reverbSize", "Size"), addKnob ("reverbMix", "Reverb"), addKnob ("volume", "Volume") }, {} },
    };

    addAndMakeVisible (tableView);

    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, moss);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, moss.withAlpha (0.4f));
    keyboard.setLowestVisibleKey (36);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (900, 560, 1800, 1120);
    getConstrainer()->setFixedAspectRatio (980.0 / 600.0);
    setSize (980, 600);
}

MoosEditor::~MoosEditor()
{
    setLookAndFeel (nullptr);
}

MoosEditor::Knob* MoosEditor::addKnob (const juce::String& paramId, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    k->slider.setPopupDisplayEnabled (false, false, this);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.0f));
    k->label.setColour (juce::Label::textColourId, dim.brighter (0.4f));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    k->slider.textFromValueFunction = [s = &k->slider] (double v)
    {
        if (s->getInterval() >= 1.0)
            return juce::String (juce::roundToInt (v));
        const double a = std::abs (v);
        return juce::String (v, a >= 100.0 ? 0 : (a >= 10.0 ? 1 : 2));
    };
    k->slider.updateText();
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void MoosEditor::layoutSection (Section& s, juce::Rectangle<int> area)
{
    s.bounds = area;
    auto inner = area.reduced (8).withTrimmedTop (18);
    const int w = inner.getWidth() / (int) s.knobs.size();
    for (auto* k : s.knobs)
    {
        auto cell = inner.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell.reduced (2, 0));
    }
}

void MoosEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    const float scale = (float) getWidth() / 980.0f;
    g.setColour (moss);
    g.setFont (juce::FontOptions (26.0f * scale, juce::Font::bold));
    g.drawText ("MOOS", 18, 6, 200, (int) (36 * scale), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.0f * scale));
    g.drawText ("wavetable pad synth  /  trockenmoos", (int) (110 * scale), 6, 400, (int) (36 * scale),
                juce::Justification::centredLeft);

    for (auto& s : sections)
    {
        auto b = s.bounds.toFloat().reduced (3.0f);
        g.setColour (panel);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (b, 6.0f, 1.0f);
        g.setColour (moss.withAlpha (0.8f));
        g.setFont (juce::FontOptions (11.0f * scale, juce::Font::bold));
        g.drawText (s.title, b.reduced (10.0f, 5.0f).toNearestInt(), juce::Justification::topLeft);
    }
}

void MoosEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    const int header = getHeight() * 40 / 600;
    r.removeFromTop (header);

    keyboard.setBounds (r.removeFromBottom (getHeight() * 70 / 600));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
    r.removeFromBottom (6);

    const int rowH = r.getHeight() / 3;
    auto row1 = r.removeFromTop (rowH);
    auto row2 = r.removeFromTop (rowH);
    auto row3 = r;

    auto unit = [] (juce::Rectangle<int> row, int units) { return row.getWidth() / units; };

    // Zeile 1: Wavetable-Anzeige | OSC A | OSC B
    const int u1 = unit (row1, 13);
    tableView.setBounds (row1.removeFromLeft (u1 * 6).reduced (5));
    layoutSection (sections[0], row1.removeFromLeft (u1 * 3));
    layoutSection (sections[1], row1);

    // Zeile 2: UNISON | FILTER | FILTER ENV
    const int u2 = unit (row2, 10);
    layoutSection (sections[2], row2.removeFromLeft (u2 * 3));
    layoutSection (sections[3], row2.removeFromLeft (u2 * 3));
    layoutSection (sections[4], row2);

    // Zeile 3: AMP ENV | LFO | FX
    const int u3 = unit (row3, 11);
    layoutSection (sections[5], row3.removeFromLeft (u3 * 4));
    layoutSection (sections[6], row3.removeFromLeft (u3 * 3));
    layoutSection (sections[7], row3);
}

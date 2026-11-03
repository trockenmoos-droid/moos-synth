#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "TmLookAndFeel.h"
#include "TmProcessor.h"
#include "TmTextures.h"

namespace tmk
{
    // Oszilloskop als Bergsilhouette (mit Spiegelung) – für Synths, die keine eigene Anzeige brauchen
    class ScopeView : public juce::Component, private juce::Timer
    {
    public:
        ScopeView (const Scope& s, const Theme& t, juce::String titleText, std::function<juce::String()> infoFn = {})
            : scope (s), theme (t), title (std::move (titleText)), info (std::move (infoFn)) { startTimerHz (30); }
        ~ScopeView() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0x50000000));
            g.fillRoundedRectangle (r, 6.0f);
            auto area = r.reduced (12.0f, 24.0f).withTrimmedTop (4.0f);

            float buf[1024];
            scope.copyLast (buf, 1024);
            // auf einen steigenden Nulldurchgang ausrichten, damit die Welle steht
            int start = 0;
            for (int i = 1; i < 512; ++i) if (buf[i - 1] < 0.0f && buf[i] >= 0.0f) { start = i; break; }
            float peak = 0.02f;
            for (int i = 0; i < 512; ++i) peak = juce::jmax (peak, std::abs (buf[start + i]));
            const float scale = 0.9f / juce::jmax (0.15f, peak);

            juce::Path wave, fill;
            const float mid = area.getCentreY();
            for (int i = 0; i < 512; ++i)
            {
                const float x = area.getX() + area.getWidth() * i / 511.0f;
                const float y = mid - buf[start + i] * scale * area.getHeight() * 0.45f;
                if (i == 0) { wave.startNewSubPath (x, y); fill.startNewSubPath (x, area.getBottom()); }
                wave.lineTo (x, y); fill.lineTo (x, y);
            }
            fill.lineTo (area.getRight(), area.getBottom());
            fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (theme.accent.withAlpha (0.35f), 0, area.getY(), theme.accent.withAlpha (0.02f), 0, area.getBottom(), false));
            g.fillPath (fill);
            g.setColour (theme.accent);
            g.strokePath (wave, juce::PathStrokeType (2.0f));

            g.setColour (theme.text);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (title, r.reduced (12.0f, 6.0f), juce::Justification::topLeft);
            if (info)
            {
                g.setColour (theme.dim);
                g.setFont (juce::FontOptions (11.0f));
                g.drawText (info(), r.reduced (12.0f, 6.0f), juce::Justification::topRight);
            }
        }

    private:
        void timerCallback() override { repaint(); }
        const Scope& scope;
        Theme theme;
        juce::String title;
        std::function<juce::String()> info;
    };

    // Gemeinsames Editor-Gerüst: Kopfzeile mit Presets und Hintergrund-Menü, Milchglas-Panels,
    // Regler-Helfer, Seiten, optionale Tastatur. Alles liegt in "content" (feste Grundgröße)
    // und wird als Ganzes skaliert.
    class Editor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        struct Config
        {
            juce::String title, subtitle, textureName;
            std::function<juce::Image()> texture;
            Theme theme;
            int width = 1000, height = 640;
            juce::StringArray pages;                       // leer = keine Seiten
            juce::MidiKeyboardState* keyboardState = nullptr;
        };

        Editor (Processor& p, Config c)
            : AudioProcessorEditor (&p), proc (p), cfg (std::move (c)), lnf (cfg.theme)
        {
            setLookAndFeel (&lnf);
            addAndMakeVisible (content);
            content.owner = this;

            for (int i = 0; i < cfg.pages.size(); ++i)
            {
                auto b = std::make_unique<juce::TextButton> (cfg.pages[i]);
                b->onClick = [this, i] { showPage (i); };
                content.addAndMakeVisible (*b);
                pageButtons.push_back (std::move (b));
            }

            presetBox.setTextWhenNothingSelected ("Preset");
            presetBox.onChange = [this] { presetSelected(); };
            content.addAndMakeVisible (presetBox);
            for (auto* b : { &prevPreset, &nextPreset, &saveButton, &bgButton }) content.addAndMakeVisible (*b);
            prevPreset.onClick = [this] { stepPreset (-1); };
            nextPreset.onClick = [this] { stepPreset (1); };
            saveButton.onClick = [this] { savePreset(); };
            bgButton.onClick   = [this] { backgroundMenu(); };

            if (cfg.keyboardState != nullptr)
            {
                keyboard = std::make_unique<juce::MidiKeyboardComponent> (*cfg.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard);
                keyboard->setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, cfg.theme.accent);
                keyboard->setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, cfg.theme.accent.withAlpha (0.4f));
                keyboard->setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colour (0x40000000));
                content.addAndMakeVisible (*keyboard);
            }

            loadBackground();
            refreshPresetList();
            startTimerHz (10);
        }

        ~Editor() override
        {
            stopTimer();
            setLookAndFeel (nullptr);
        }

        // Muss von der Unterklasse am Ende ihres Konstruktors aufgerufen werden
        void finishSetup()
        {
            showPage (0);
            layoutAll();
            setResizable (true, true);
            setResizeLimits (cfg.width * 7 / 10, cfg.height * 7 / 10, cfg.width * 2, cfg.height * 2);
            getConstrainer()->setFixedAspectRatio ((double) cfg.width / cfg.height);
            setSize (cfg.width, cfg.height);
        }

        void showPage (int page)
        {
            currentPage = page;
            for (size_t i = 0; i < pageButtons.size(); ++i) pageButtons[i]->setToggleState ((int) i == page, juce::dontSendNotification);
            for (auto& s : sections)
            {
                const bool vis = s.page < 0 || s.page == page;
                for (auto& it : s.items)
                {
                    it.comp->setVisible (vis);
                    if (it.label != nullptr) it.label->setVisible (vis);
                }
                for (auto* b : s.headerButtons) b->setVisible (vis);
            }
            pageChanged (page);
            content.repaint();
        }

        void resized() override
        {
            content.setBounds (0, 0, cfg.width, cfg.height);
            content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) cfg.width));
        }

        void paint (juce::Graphics& g) override { g.fillAll (cfg.theme.bg); }

    protected:
        using APVTS = juce::AudioProcessorValueTreeState;

        struct Item
        {
            juce::Component* comp = nullptr;
            juce::Label* label = nullptr;
            float weight = 1.0f;
            int kind = 0;   // 0 Drehregler, 1 Liste/Schalter, 2 frei
        };

        struct Section
        {
            juce::String title;
            int page = -1;   // -1 = immer sichtbar
            std::vector<Item> items;
            int columns = 0;  // > 0: Elemente in einem Raster mit so vielen Spalten
            std::vector<juce::Component*> headerButtons;
            juce::Rectangle<int> bounds;
        };

        // --- von Unterklassen zu überschreiben ---
        virtual void layoutBody (juce::Rectangle<int> body) = 0;
        virtual void paintOverlay (juce::Graphics&) {}
        virtual void pageChanged (int) {}
        virtual void tick() {}

        // --- Bausteine ---
        Item knob (const juce::String& id, const juce::String& text, float weight = 1.0f)
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
            Item it { s.get(), makeLabel (text), weight, 0 };
            owned.push_back (std::move (s));
            return it;
        }

        Item combo (const juce::String& id, const juce::String& text, float weight = 1.3f)
        {
            auto c = std::make_unique<juce::ComboBox>();
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
                c->addItemList (p->choices, 1);
            content.addAndMakeVisible (*c);
            comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, *c));
            Item it { c.get(), makeLabel (text), weight, 1 };
            owned.push_back (std::move (c));
            return it;
        }

        Item toggle (const juce::String& id, const juce::String& text, float weight = 0.9f)
        {
            auto b = std::make_unique<juce::TextButton> (text);
            b->setClickingTogglesState (true);
            content.addAndMakeVisible (*b);
            buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, *b));
            Item it { b.get(), makeLabel (" "), weight, 1 };
            owned.push_back (std::move (b));
            return it;
        }

        Item custom (juce::Component& c, float weight)
        {
            content.addAndMakeVisible (c);
            return { &c, nullptr, weight, 2 };
        }

        Section& addSection (const juce::String& title, std::vector<Item> items, int page = -1)
        {
            sections.push_back ({ title, page, std::move (items), {}, {} });
            return sections.back();
        }

        Section& section (const juce::String& title)
        {
            for (auto& s : sections) if (s.title == title) return s;
            jassertfalse;
            return sections.front();
        }

        void layoutSection (const juce::String& title, juce::Rectangle<int> area)
        {
            auto& s = section (title);
            s.bounds = area;
            auto header = area.reduced (8).removeFromTop (20);
            for (int i = (int) s.headerButtons.size(); --i >= 0;)
            {
                s.headerButtons[(size_t) i]->setBounds (header.removeFromRight (56).reduced (1, 0));
                header.removeFromRight (4);
            }
            auto inner = area.reduced (10, 8).withTrimmedTop (22);
            if (s.columns > 0)
            {
                const int rows = ((int) s.items.size() + s.columns - 1) / s.columns;
                const int cw = inner.getWidth() / s.columns, ch = inner.getHeight() / juce::jmax (1, rows);
                for (size_t i = 0; i < s.items.size(); ++i)
                {
                    auto& it = s.items[i];
                    auto cell = juce::Rectangle<int> (inner.getX() + (int) (i % (size_t) s.columns) * cw,
                                                      inner.getY() + (int) (i / (size_t) s.columns) * ch, cw, ch);
                    if (it.label != nullptr) it.label->setBounds (cell.removeFromTop (15));
                    const int size = juce::jmin (cell.getWidth() - 6, cell.getHeight());
                    it.comp->setBounds (cell.withSizeKeepingCentre (juce::jmax (54, size), cell.getHeight()));
                }
                return;
            }
            float total = 0.0f;
            for (auto& it : s.items) total += it.weight;
            const int fullW = inner.getWidth();
            for (auto& it : s.items)
            {
                auto cell = inner.removeFromLeft ((int) std::round (fullW * it.weight / total));
                if (it.kind == 2) { it.comp->setBounds (cell.reduced (4, 2)); continue; }
                if (it.label != nullptr) it.label->setBounds (cell.removeFromTop (16));
                if (it.kind == 0)
                    it.comp->setBounds (cell.withSizeKeepingCentre (juce::jlimit (60, 92, cell.getWidth() - 4), juce::jmin (cell.getHeight(), 112)));
                else
                    it.comp->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 10, 28).translated (0, -10));
            }
        }

        static std::vector<juce::Rectangle<int>> split (juce::Rectangle<int> a, std::initializer_list<int> w)
        {
            std::vector<juce::Rectangle<int>> out;
            int total = 0; for (int x : w) total += x;
            const int fullW = a.getWidth();
            for (int x : w) out.push_back (a.removeFromLeft (fullW * x / total));
            return out;
        }

        juce::Label* makeLabel (const juce::String& text)
        {
            auto l = std::make_unique<juce::Label> (juce::String(), text);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::FontOptions (12.0f));
            l->setColour (juce::Label::textColourId, cfg.theme.dim.brighter (0.3f));
            l->setInterceptsMouseClicks (false, false);
            content.addAndMakeVisible (*l);
            labels.push_back (std::move (l));
            return labels.back().get();
        }

        static juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

        Processor& proc;
        Config cfg;
        LookAndFeel lnf;
        int currentPage = 0;

        struct Content : juce::Component
        {
            Editor* owner = nullptr;
            void paint (juce::Graphics& g) override { owner->paintContent (g); }
        } content;

        juce::Image bgImage, bgBlur;

    private:
        void layoutAll()
        {
            auto r = juce::Rectangle<int> (0, 0, cfg.width, cfg.height).reduced (12, 8);
            auto head = r.removeFromTop (48);
            head.removeFromLeft (titleWidth());
            for (auto& b : pageButtons) b->setBounds (head.removeFromLeft (112).withSizeKeepingCentre (106, 30));
            bgButton.setBounds (head.removeFromRight (108).withSizeKeepingCentre (104, 30));
            head.removeFromRight (8);
            saveButton.setBounds (head.removeFromRight (98).withSizeKeepingCentre (94, 30));
            head.removeFromRight (8);
            nextPreset.setBounds (head.removeFromRight (32).withSizeKeepingCentre (30, 30));
            presetBox.setBounds (head.removeFromRight (230).withSizeKeepingCentre (226, 30));
            prevPreset.setBounds (head.removeFromRight (32).withSizeKeepingCentre (30, 30));

            if (keyboard != nullptr)
            {
                keyboard->setBounds (r.removeFromBottom (78));
                keyboard->setKeyWidth ((float) keyboard->getWidth() / 36.0f);
                keyboard->setOctaveForMiddleC (4);
                keyboard->setLowestVisibleKey (36);
                r.removeFromBottom (6);
            }
            layoutBody (r);
        }

        int titleTextWidth() const { return juce::jmax (110, cfg.title.length() * 20 + 16); }
        int titleWidth() const { return titleTextWidth() + 130; }

        void paintContent (juce::Graphics& g)
        {
            const auto full = juce::Rectangle<float> (0, 0, (float) cfg.width, (float) cfg.height);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            if (bgImage.isValid()) g.drawImage (bgImage, full, juce::RectanglePlacement::fillDestination);
            else                   g.fillAll (cfg.theme.bg);

            g.setGradientFill (juce::ColourGradient (juce::Colour (0xc0050505), 0, 0, juce::Colour (0x00050505), 0, 70, false));
            g.fillRect (full.withHeight (70));

            g.setColour (cfg.theme.accent);
            g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
            const int tw = titleTextWidth();
            g.drawText (cfg.title, 18, 10, tw, 40, juce::Justification::centredLeft);
            g.setColour (cfg.theme.text.withAlpha (0.7f));
            g.setFont (juce::FontOptions (10.5f));
            g.drawText (cfg.subtitle, 20 + tw, 14, 110, 16, juce::Justification::centredLeft);
            g.drawText ("trockenmoos audio", 20 + tw, 28, 110, 16, juce::Justification::centredLeft);

            for (auto& s : sections)
            {
                if (! (s.page < 0 || s.page == currentPage) || s.bounds.isEmpty()) continue;
                auto b = s.bounds.toFloat().reduced (4.0f);
                {
                    juce::Graphics::ScopedSaveState ss (g);
                    juce::Path clip;
                    clip.addRoundedRectangle (b, 8.0f);
                    g.reduceClipRegion (clip);
                    if (bgBlur.isValid()) g.drawImage (bgBlur, full, juce::RectanglePlacement::fillDestination);
                    g.setColour (cfg.theme.glass.withMultipliedAlpha (glassFactor));
                    g.fillRect (b);
                }
                g.setColour (cfg.theme.outline);
                g.drawRoundedRectangle (b, 8.0f, 1.0f);
                g.setColour (cfg.theme.accent);
                g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
                g.drawText (s.title, b.reduced (12.0f, 7.0f).toNearestInt(), juce::Justification::topLeft);
            }

            if (keyboard != nullptr)
            {
                g.setColour (juce::Colour (0xa0000000));
                g.fillRoundedRectangle (keyboard->getBounds().toFloat().expanded (3.0f), 4.0f);
            }
            paintOverlay (g);
        }

        void timerCallback() override
        {
            if (waitingForTexture && textures->get (texKey, texFn, bgImage, bgBlur))
            {
                waitingForTexture = false;
                content.repaint();
            }
            if (proc.presetChangeCounter.load() != lastPresetCounter)
            {
                lastPresetCounter = proc.presetChangeCounter.load();
                refreshPresetList();
                content.repaint();
            }
            tick();
        }

        // --- Presets ---
        void refreshPresetList()
        {
            presetBox.clear (juce::dontSendNotification);
            presetBox.addSectionHeading ("Werkspresets");
            const auto& f = proc.getFactoryPresets();
            for (int i = 0; i < (int) f.size(); ++i) presetBox.addItem (u8 (f[(size_t) i].name), i + 1);
            userFiles = proc.userPresets();
            if (! userFiles.isEmpty())
            {
                presetBox.addSeparator();
                presetBox.addSectionHeading ("Eigene Presets");
                for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), 1000 + i);
            }
            const auto name = proc.getPresetName();
            int id = 0;
            for (int i = 0; i < presetBox.getNumItems(); ++i) if (presetBox.getItemText (i) == name) id = presetBox.getItemId (i);
            if (id != 0) presetBox.setSelectedId (id, juce::dontSendNotification);
            else         presetBox.setText (name, juce::dontSendNotification);
        }

        void presetSelected()
        {
            const int id = presetBox.getSelectedId();
            if (id <= 0) return;
            if (id < 1000) proc.loadFactoryPreset (id - 1);
            else if (juce::isPositiveAndBelow (id - 1000, userFiles.size()))
            {
                const auto err = proc.loadUserPreset (userFiles[id - 1000]);
                if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", err);
            }
        }

        void stepPreset (int delta)
        {
            const int n = presetBox.getNumItems();
            if (n == 0) return;
            int idx = 0;
            for (int i = 0; i < n; ++i) if (presetBox.getItemId (i) == presetBox.getSelectedId()) idx = i;
            idx = (idx + delta + n) % n;
            presetBox.setSelectedId (presetBox.getItemId (idx), juce::sendNotificationSync);
        }

        void savePreset()
        {
            chooser = std::make_unique<juce::FileChooser> ("Preset speichern",
                proc.userPresetFolder().getChildFile (proc.getPresetName() + ".tmpreset"), "*.tmpreset");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      auto f = fc.getResult();
                                      if (f == juce::File()) return;
                                      if (! proc.saveUserPreset (f.withFileExtension ("tmpreset")))
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset",
                                                                                  "Speichern fehlgeschlagen.");
                                  });
        }

        // --- Hintergrund ---
        void backgroundMenu()
        {
            tex::showBackgroundMenu (bgButton, cfg.textureName, cfg.textureName, [this] { loadBackground(); }, chooser);
        }

        void loadBackground()
        {
            waitingForTexture = false;
            glassFactor = tex::glassFactor (tex::getDurchsicht());
            const auto choice = tex::getBackground (cfg.textureName);
            texKey = cfg.textureName;
            texFn  = cfg.texture;
            if (choice.startsWith ("tex:"))
            {
                texKey = choice.fromFirstOccurrenceOf ("tex:", false, false);
                texFn  = tex::landscapeFor (texKey);
            }
            else if (choice.isNotEmpty())
            {
                auto img = tex::fromPhoto (juce::File (choice));
                if (img.isValid()) { bgImage = img; bgBlur = tex::blurred (img); content.repaint(); return; }
            }
            if (! textures->get (texKey, texFn, bgImage, bgBlur))
            {
                bgImage = {}; bgBlur = {};
                waitingForTexture = true;
            }
            content.repaint();
        }

        juce::String texKey;
        std::function<juce::Image()> texFn;
        float glassFactor = 0.8f;

        juce::SharedResourcePointer<tex::Service> textures;
        bool waitingForTexture = false;
        int lastPresetCounter = -1;

        std::vector<std::unique_ptr<juce::Component>> owned;
        std::vector<std::unique_ptr<juce::Label>> labels;
        std::vector<std::unique_ptr<APVTS::SliderAttachment>> sliderAtts;
        std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAtts;
        std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;
        std::vector<std::unique_ptr<juce::TextButton>> pageButtons;

    protected:
        std::list<Section> sections;     // std::list: Referenzen bleiben gültig

    private:
        juce::ComboBox presetBox;
        juce::Array<juce::File> userFiles;
        juce::TextButton prevPreset { "<" }, nextPreset { ">" }, saveButton { "SPEICHERN" }, bgButton { "HINTERGRUND" };
        std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
        std::unique_ptr<juce::FileChooser> chooser;
    };
}

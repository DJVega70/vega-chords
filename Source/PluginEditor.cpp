#include "PluginEditor.h"
using namespace vch;

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
class ChordPad : public juce::Component
{
public:
    ChordPad (VegaChordsProcessor& p, int i) : proc (p), index (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void (int)> onClick;
    bool highlighted = false;
    void paint (juce::Graphics& g) override
    {
        const auto pal = proc.palette();
        const auto& c = pal[(size_t) index];
        auto b = getLocalBounds().toFloat().reduced (3.0f);
        const bool colour = index >= 7;
        g.setColour (juce::Colour (isMouseOver() ? 0xff2c2c2c : 0xff202020)); g.fillRoundedRectangle (b, 5.0f);
        g.setColour (highlighted ? VegaColours::gold : (colour ? VegaColours::gold.withAlpha (0.35f) : VegaColours::accent.withAlpha (0.45f)));
        g.drawRoundedRectangle (b, 5.0f, highlighted ? 2.0f : 1.0f);
        g.setColour (colour ? VegaColours::gold : VegaColours::accent); g.setFont (VegaFonts::ui (19.0f, true));
        g.drawText (numeral (c, proc.key()), b.withTrimmedBottom (b.getHeight() * 0.42f).toNearestInt(), juce::Justification::centredBottom);
        g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (12.5f));
        g.drawText (c.name (proc.key(), proc.scale()), b.withTrimmedTop (b.getHeight() * 0.58f).toNearestInt(), juce::Justification::centredTop);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mouseWasDraggedSinceMouseDown() && onClick) onClick (index); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 6)
            if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
                if (! c->isDragAndDropActive()) c->startDragging ("pad:" + juce::String (index), this);
    }
    VegaChordsProcessor& proc; int index;
};

//==============================================================================
class ProgressionLane : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit ProgressionLane (VegaChordsProcessor& p) : proc (p) {}
    int selected = 0;
    std::function<void()> onChange;

    juce::Rectangle<float> slotRect (int i) const
    {
        const int n = proc.numSlots.load();
        const float total = (float) std::max (1, [&] { int t = 0; for (int k = 0; k < n; ++k) t += proc.slots[(size_t) k].beats.load(); return t; }());
        auto b = getLocalBounds().toFloat().reduced (6.0f).withTrimmedRight (70.0f);
        float x = b.getX();
        for (int k = 0; k < i; ++k) x += b.getWidth() * proc.slots[(size_t) k].beats.load() / total;
        return { x, b.getY(), b.getWidth() * proc.slots[(size_t) i].beats.load() / total, b.getHeight() };
    }
    juce::Rectangle<float> addRect() const { auto b = getLocalBounds().toFloat().reduced (6.0f); return b.removeFromRight (62.0f); }

    void paint (juce::Graphics& g) override
    {
        vegaui::drawScreen (g, getLocalBounds().toFloat());
        const int n = proc.numSlots.load(), playing = proc.playingSlot.load();
        for (int i = 0; i < n; ++i)
        {
            auto r = slotRect (i).reduced (2.0f);
            const auto c = proc.getSlot (i);
            g.setColour (juce::Colour (i == dropHover ? 0xff2e3a3a : 0xff1d1d1d)); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (i == playing ? VegaColours::gold : (i == selected ? VegaColours::accent : VegaColours::line));
            g.drawRoundedRectangle (r, 4.0f, i == playing || i == selected ? 2.0f : 1.0f);
            if (c.valid())
            {
                g.setColour (VegaColours::accent); g.setFont (VegaFonts::ui (r.getWidth() > 90 ? 22.0f : 15.0f, true));
                g.drawText (numeral (c, proc.key()), r.withTrimmedBottom (r.getHeight() * 0.45f).toNearestInt(), juce::Justification::centredBottom);
                g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (13.0f));
                g.drawText (c.name (proc.key(), proc.scale()), r.withTrimmedTop (r.getHeight() * 0.55f).withTrimmedBottom (18).toNearestInt(), juce::Justification::centredTop);
            }
            else { g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f)); g.drawText ("drop a chord", r.toNearestInt(), juce::Justification::centred); }
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f));
            const int beats = proc.slots[(size_t) i].beats.load();
            g.drawText (juce::String (beats) + (beats == 1 ? " beat" : " beats"), r.removeFromBottom (16).toNearestInt(), juce::Justification::centred);
        }
        if (playing >= 0 && playing < n)
        {   // playhead
            double start = 0; for (int k = 0; k < playing; ++k) start += proc.slots[(size_t) k].beats.load();
            const double frac = (proc.uiPpq.load() - start) / std::max (1, proc.slots[(size_t) playing].beats.load());
            auto r = slotRect (playing);
            g.setColour (VegaColours::gold); g.fillRect ((float) (r.getX() + r.getWidth() * juce::jlimit (0.0, 1.0, frac)), r.getY(), 2.0f, r.getHeight());
        }
        auto a = addRect();
        g.setColour (n < kMaxSlots ? VegaColours::textDim : VegaColours::line); g.drawRoundedRectangle (a.reduced (2.0f), 4.0f, 1.0f);
        g.setFont (VegaFonts::ui (22.0f)); g.drawText ("+", a.toNearestInt(), juce::Justification::centred);
    }

    int slotAt (juce::Point<float> p) const { for (int i = 0; i < proc.numSlots.load(); ++i) if (slotRect (i).contains (p)) return i; return -1; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (addRect().contains (e.position)) { addSlot(); return; }
        const int i = slotAt (e.position); if (i < 0) return;
        selected = i;
        if (e.mods.isPopupMenu()) { menu (i); return; }
        const auto c = proc.getSlot (i); if (c.valid()) proc.audition (c);
        changed();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int i = slotAt (e.position); if (i < 0) return;
        auto& b = proc.slots[(size_t) i].beats; b.store (juce::jlimit (1, 16, b.load() + (w.deltaY > 0 ? 1 : -1))); proc.touch(); changed();
    }
    void menu (int i)
    {
        juce::PopupMenu m, len;
        for (int b : { 1, 2, 3, 4, 6, 8, 16 }) len.addItem (100 + b, juce::String (b) + (b == 1 ? " beat" : " beats"), true, proc.slots[(size_t) i].beats.load() == b);
        m.addSubMenu ("Length", len);
        m.addItem (1, "Duplicate"); m.addItem (2, "Insert empty before"); m.addItem (3, "Clear chord"); m.addItem (4, "Delete slot");
        m.showMenuAsync (juce::PopupMenu::Options(), [this, i] (int r)
        {
            const int n = proc.numSlots.load();
            auto copy = [&] (int from, int to) { proc.setSlot (to, proc.getSlot (from), proc.slots[(size_t) from].beats.load()); };
            if (r > 100) { proc.slots[(size_t) i].beats.store (r - 100); proc.touch(); }
            else if (r == 1 && n < kMaxSlots) { for (int k = n; k > i; --k) copy (k - 1, k); proc.numSlots.store (n + 1); }
            else if (r == 2 && n < kMaxSlots) { for (int k = n; k > i; --k) copy (k - 1, k); proc.setSlot (i, {}, 4); proc.numSlots.store (n + 1); }
            else if (r == 3) proc.setSlot (i, {});
            else if (r == 4 && n > 1) { for (int k = i; k < n - 1; ++k) copy (k + 1, k); proc.numSlots.store (n - 1); selected = std::min (selected, n - 2); }
            proc.touch(); changed();
        });
    }
    void addSlot()
    {
        const int n = proc.numSlots.load(); if (n >= kMaxSlots) return;
        proc.setSlot (n, {}, 4); proc.numSlots.store (n + 1); selected = n; proc.touch(); changed();
    }
    void place (const Chord& c)
    {
        if (proc.numSlots.load() == 0) addSlot();
        proc.setSlot (selected, c);
        if (selected < proc.numSlots.load() - 1) ++selected;     // step forward, like a sequencer
        changed();
    }

    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("pad:"); }
    void itemDragMove (const SourceDetails& d) override { dropHover = slotAt (d.localPosition.toFloat()); repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        dropHover = -1;
        const int pad = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
        int i = slotAt (d.localPosition.toFloat());
        if (i < 0 && addRect().contains (d.localPosition.toFloat())) { addSlot(); i = proc.numSlots.load() - 1; }
        if (i < 0) return;
        proc.setSlot (i, proc.palette()[(size_t) pad]); selected = i; changed();
    }
private:
    void changed() { repaint(); if (onChange) onChange(); }
    VegaChordsProcessor& proc; int dropHover = -1;
};

//==============================================================================
// Drag this out into FL Studio's playlist / piano roll (or any DAW) to get the progression as MIDI
class MidiDragSource : public juce::Component
{
public:
    explicit MidiDragSource (VegaChordsProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (VegaColours::accent.withAlpha (isMouseOver() ? 0.35f : 0.2f)); g.fillRoundedRectangle (b, 4.0f);
        g.setColour (VegaColours::accent); g.drawRoundedRectangle (b, 4.0f, 1.2f);
        g.setFont (VegaFonts::ui (11.5f, true)); g.drawText (juce::String::fromUTF8 ("\xe2\x87\xb1  DRAG MIDI"), b.toNearestInt(), juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        const auto f = proc.exportMidi();
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    VegaChordsProcessor& proc; bool dragging = false;
};
} // namespace

//==============================================================================
class VegaChordsEditor::Main : public juce::Component, private juce::ChangeListener
{
public:
    Main (VegaChordsProcessor& p) : proc (p), a (p.apvts), lane (p), midiDrag (p),
        key (a, "key", "Key"), scale (a, "scale", "Scale"), colour (a, "extension", "Chord Colour"), genStyle (a, "genStyle", "Style"),
        reg (a, "register", "Register"), inversion (a, "inversion", "Inversion"), voicingSel (a, "voicing", "Voicing"), lead (a, "voiceLead", "Voice Leading"),
        bass (a, "bass", "Bass Note"), bassReg (a, "bassReg", "Bass Register"),
        pattern (a, "pattern", "Pattern"), gate (a, "gate", "Gate"), strum (a, "strum", "Strum"), strumDir (a, "strumDir", "Strum Dir"),
        humanT (a, "humanTime", "Humanize Time"), humanV (a, "humanVel", "Humanize Vel"), vel (a, "velocity", "Velocity"),
        midiCh (a, "midiCh", "Chord Ch"), bassCh (a, "bassCh", "Bass Ch"), trigger (a, "trigger", "Keyboard"), preview (a, "preview", "Built-in Sound"),
        previewVol (a, "previewVol", "Level"), tempo (a, "tempo", "Tempo"),
        keyboard (p.outKeyboard, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        for (int i = 0; i < 14; ++i)
        {
            pads[(size_t) i] = std::make_unique<ChordPad> (p, i);
            pads[(size_t) i]->onClick = [this] (int idx) { const auto c = proc.palette()[(size_t) idx]; proc.audition (c); lane.place (c); };
            addAndMakeVisible (*pads[(size_t) i]);
        }
        lane.onChange = [this] { updateSuggestions(); };
        addAndMakeVisible (lane); addAndMakeVisible (midiDrag);
        for (auto* c : std::initializer_list<juce::Component*> { &key, &scale, &colour, &genStyle, &reg, &inversion, &voicingSel, &lead, &bass, &bassReg, &pattern, &gate, &strum,
                                                                 &strumDir, &humanT, &humanV, &vel, &midiCh, &bassCh, &trigger, &preview, &previewVol, &tempo, &keyboard })
            addAndMakeVisible (c);
        keyboard.setAvailableRange (24, 96); keyboard.setKeyWidth (18.0f); keyboard.setInterceptsMouseClicks (false, false);
        auto btn = [&] (juce::TextButton& b, const char* t, std::function<void()> f) { b.setButtonText (t); b.onClick = std::move (f); addAndMakeVisible (b); };
        btn (generate, "GENERATE", [this] { proc.generate (juce::roundToInt (a.getRawParameterValue ("genStyle")->load())); lane.selected = 0; afterChange(); });
        btn (remix, "REMIX COLOUR", [this] { proc.randomizeColour(); afterChange(); });
        btn (clear, "CLEAR", [this] { for (int i = 0; i < kMaxSlots; ++i) proc.setSlot (i, {}, 4); proc.numSlots.store (4); lane.selected = 0; afterChange(); });
        btn (play, "PLAY", [this] { proc.internalPlay.store (play.getToggleState()); });
        play.setClickingTogglesState (true); play.setColour (juce::TextButton::buttonOnColourId, VegaColours::accent);
        btn (saveMidi, "SAVE MIDI...", [this] { saveMidiFile(); });
        btn (detectKey, "DETECT KEY", [this] { detectFromPlaying(); });
        btn (loadBtn, "LOAD", [this] { loadPreset(); });
        btn (saveBtn, "SAVE", [this] { savePreset(); });
        a.addParameterListener ("key", &listener); a.addParameterListener ("scale", &listener); a.addParameterListener ("extension", &listener);
        {
            juce::Component::SafePointer<Main> safe (this);
            listener.onChange = [safe] { juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->afterChange(); }); };
        }
        updateSuggestions();
    }
    ~Main() override { a.removeParameterListener ("key", &listener); a.removeParameterListener ("scale", &listener); a.removeParameterListener ("extension", &listener); }

    void refresh()
    {
        lane.repaint();
        const int pl = proc.playingSlot.load();
        for (auto& pd : pads)
        {
            bool h = false;
            if (pl >= 0) { const auto c = proc.getSlot (pl); const auto pc = proc.palette()[(size_t) pd->index]; h = c.root == pc.root && c.quality == pc.quality; }
            if (h != pd->highlighted) { pd->highlighted = h; pd->repaint(); }
        }
        const int r = proc.detectedRoot.load();
        const juce::String det = r >= 0 ? Chord { r, proc.detectedQuality.load(), {} }.name (proc.key(), proc.scale()) : juce::String();
        if (det != detected) { detected = det; repaint (W - 520, 0, 250, 34); }
        if (! proc.internalPlay.load() && play.getToggleState()) play.setToggleState (false, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (VegaColours::background);
        vegaui::drawTopBar (g, W, "CHORDS  VP-9");
        vegaui::drawBody (g, { 0, 35, W, H - 65 });
        g.setFont (VegaFonts::ui (11.0f, true)); g.setColour (detected.isNotEmpty() ? VegaColours::accent : VegaColours::textDim);
        g.drawText (detected.isNotEmpty() ? "YOU'RE PLAYING: " + detected : "PLAY A CHORD TO NAME IT", W - 520, 0, 250, 34, juce::Justification::centredRight);
        vegaui::drawSection (g, { 0, 40, 520, 70 }, "KEY  /  COLOUR", false);
        g.setFont (VegaFonts::ui (10.0f, true)); g.setColour (VegaColours::textDim);
        g.drawText ("DIATONIC", 12, 116, 120, 14, juce::Justification::left);
        g.setColour (VegaColours::gold.withAlpha (0.8f));
        g.drawText ("COLOUR  -  BORROWED / SECONDARY DOMINANTS / NEAPOLITAN", 12, 206, 500, 14, juce::Justification::left);
        g.setColour (VegaColours::textDim);
        g.drawText ("PROGRESSION  -  click a pad to place it, drag pads onto slots, right-click a slot for length / delete, scroll to change length", 12, 304, 900, 14, juce::Justification::left);
        g.drawText ("NEXT CHORD IDEAS", 12, 462, 200, 14, juce::Justification::left);
        g.setColour (VegaColours::line); g.drawHorizontalLine (494, 0, (float) W);
        vegaui::drawSection (g, { 0, 498, 470, 170 }, "VOICING", false);
        vegaui::drawSection (g, { 470, 498, 520, 170 }, "RHYTHM  &  FEEL");
        vegaui::drawSection (g, { 990, 498, W - 990, 170 }, "OUTPUT");
        vegaui::drawBottomBar (g, { 0, H - 30, W, 30 }, "SET FL STUDIO'S MIDI OUT PORT (WRAPPER SETTINGS) TO DRIVE OTHER INSTRUMENTS  -  OR DRAG MIDI STRAIGHT INTO THE PLAYLIST");
    }

    void resized() override
    {
        loadBtn.setBounds (W - 260, 7, 60, 20); saveBtn.setBounds (W - 196, 7, 60, 20);
        key.setBounds (12, 56, 80, 46); scale.setBounds (98, 56, 140, 46); colour.setBounds (244, 56, 120, 46); detectKey.setBounds (372, 72, 110, 24);
        genStyle.setBounds (560, 56, 140, 46);
        generate.setBounds (708, 72, 100, 24); remix.setBounds (812, 72, 110, 24); clear.setBounds (926, 72, 70, 24);
        play.setBounds (1010, 72, 60, 24); tempo.setBounds (1074, 40, 80, 70);
        midiDrag.setBounds (1160, 50, 128, 24); saveMidi.setBounds (1160, 80, 128, 22);
        const int pw = (W - 24) / 7;
        for (int i = 0; i < 14; ++i) pads[(size_t) i]->setBounds (12 + (i % 7) * pw, i < 7 ? 132 : 222, pw, 76);
        lane.setBounds (10, 320, W - 20, 136);
        int x = 150; for (auto& s : sugg) { s->setBounds (x, 457, 110, 26); x += 116; }
        // voicing
        reg.setBounds (12, 518, 90, 46); inversion.setBounds (108, 512, 80, 110); voicingSel.setBounds (196, 518, 110, 46); lead.setBounds (316, 512, 70, 74);
        bass.setBounds (390, 512, 70, 74); bassReg.setBounds (12, 590, 90, 46);
        // rhythm
        pattern.setBounds (482, 518, 150, 46); strumDir.setBounds (482, 590, 150, 46);
        gate.setBounds (640, 512, 68, 110); strum.setBounds (708, 512, 68, 110); humanT.setBounds (776, 512, 70, 110); humanV.setBounds (846, 512, 70, 110);
        vel.setBounds (916, 512, 68, 110);
        // output
        midiCh.setBounds (998, 512, 70, 100); bassCh.setBounds (1068, 512, 70, 100); trigger.setBounds (1144, 518, 146, 46);
        preview.setBounds (1150, 580, 70, 74); previewVol.setBounds (1220, 574, 70, 96);
        keyboard.setBounds (10, 676, W - 20, 92); keyboard.setKeyWidth ((W - 20) / 43.0f);
    }

private:
    void afterChange() { for (auto& p : pads) p->repaint(); lane.repaint(); updateSuggestions(); }
    void updateSuggestions()
    {
        for (auto& s : sugg) removeChildComponent (s.get());
        sugg.clear();
        int ref = -1;
        for (int i = std::min (lane.selected, proc.numSlots.load() - 1); i >= 0; --i) if (proc.getSlot (i).valid()) { ref = i; break; }
        if (ref < 0) { for (int i = proc.numSlots.load() - 1; i >= 0; --i) if (proc.getSlot (i).valid()) { ref = i; break; } }
        if (ref < 0) return;
        const auto c = proc.getSlot (ref);
        const auto pal = proc.palette();
        for (int semis : nextRoots (((c.root - proc.key()) % 12 + 12) % 12, scaleIsMinor (proc.scale())))
        {
            const int root = (proc.key() + semis) % 12;
            for (int i = 0; i < 14; ++i)
                if (pal[(size_t) i].root == root)
                {
                    auto b = std::make_unique<juce::TextButton> (numeral (pal[(size_t) i], proc.key()) + "  " + pal[(size_t) i].name (proc.key(), proc.scale()));
                    b->setColour (juce::TextButton::buttonColourId, i >= 7 ? VegaColours::gold.withAlpha (0.35f) : VegaColours::accentDim);
                    b->onClick = [this, i] { const auto ch = proc.palette()[(size_t) i]; proc.audition (ch); lane.place (ch); };
                    addAndMakeVisible (*b); sugg.push_back (std::move (b));
                    break;
                }
        }
        resized();
    }
    void detectFromPlaying()
    {
        std::array<float, 12> h {}; for (int i = 0; i < 12; ++i) h[(size_t) i] = proc.inputHist[(size_t) i].load();
        auto [k, minor] = vch::detectKey (h);
        if (k < 0) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Detect key", "Play a few notes or chords on your MIDI keyboard first, then press Detect Key."); return; }
        if (auto* kp = a.getParameter ("key")) kp->setValueNotifyingHost (kp->convertTo0to1 ((float) k));
        if (auto* sp = a.getParameter ("scale")) sp->setValueNotifyingHost (sp->convertTo0to1 (minor ? 1.0f : 0.0f));
        for (auto& h2 : proc.inputHist) h2.store (0);
    }
    void saveMidiFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save progression as MIDI", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Chords.mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) proc.exportMidi (fc.getResult().withFileExtension (".mid")); });
    }
    void savePreset()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save chord set", VegaChordsProcessor::presetFolder().getChildFile ("My Progression.vegachords"), "*.vegachords");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) proc.savePreset (fc.getResult().withFileExtension (".vegachords")); });
    }
    void loadPreset()
    {
        juce::PopupMenu m; auto files = VegaChordsProcessor::presetFolder().findChildFiles (juce::File::findFiles, false, "*.vegachords"); files.sort();
        for (int i = 0; i < files.size(); ++i) m.addItem (1 + i, files[i].getFileNameWithoutExtension());
        if (files.isEmpty()) m.addItem (-1, "No saved chord sets yet - use SAVE", false);
        m.addSeparator(); m.addItem (999, "Show folder");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadBtn), [this, files] (int r)
        {
            if (r == 999) VegaChordsProcessor::presetFolder().revealToUser();
            else if (r > 0) { proc.loadPreset (files[r - 1]); lane.selected = 0; afterChange(); }
        });
    }
    void changeListenerCallback (juce::ChangeBroadcaster*) override {}

    struct Listener : juce::AudioProcessorValueTreeState::Listener { std::function<void()> onChange; void parameterChanged (const juce::String&, float) override { if (onChange) onChange(); } } listener;
    VegaChordsProcessor& proc; APVTS& a;
    std::array<std::unique_ptr<ChordPad>, 14> pads; ProgressionLane lane; MidiDragSource midiDrag;
    vegaui::Choice key, scale, colour, genStyle, reg; vegaui::Knob inversion; vegaui::Choice voicingSel; vegaui::Toggle lead, bass; vegaui::Choice bassReg;
    vegaui::Choice pattern; vegaui::Knob gate, strum; vegaui::Choice strumDir; vegaui::Knob humanT, humanV, vel, midiCh, bassCh; vegaui::Choice trigger;
    vegaui::Toggle preview; vegaui::Knob previewVol, tempo;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton generate, remix, clear, play, saveMidi, detectKey, loadBtn, saveBtn;
    std::vector<std::unique_ptr<juce::TextButton>> sugg;
    std::unique_ptr<juce::FileChooser> chooser; juce::String detected;
};

//==============================================================================
VegaChordsEditor::VegaChordsEditor (VegaChordsProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    main = std::make_unique<Main> (p);
    addAndMakeVisible (*main);
    constrainer.setFixedAspectRatio ((double) W / H);
    constrainer.setSizeLimits (W / 2, H / 2, W * 2, H * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const float s = juce::jlimit (0.5f, 2.0f, proc.uiScale.load());
    setSize (juce::roundToInt (W * s), juce::roundToInt (H * s));
    startTimerHz (20);
}
VegaChordsEditor::~VegaChordsEditor() { stopTimer(); setConstrainer (nullptr); main.reset(); setLookAndFeel (nullptr); }
void VegaChordsEditor::timerCallback() { main->refresh(); }
void VegaChordsEditor::resized()
{
    main->setBounds (0, 0, W, H);
    const float s = (float) getWidth() / (float) W;
    main->setTransform (juce::AffineTransform::scale (s));
    proc.uiScale.store (s);
}

juce::AudioProcessorEditor* VegaChordsProcessor::createEditor() { return new VegaChordsEditor (*this); }

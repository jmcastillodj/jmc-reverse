#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "dsp/Timing.h"
#include "ExportNaming.h"

using jmc::EngineState;
namespace ui = jmc::ui;
namespace col = jmc::ui::col;

namespace
{
constexpr int kWinW = 1000, kAdvW = 496, kWinH = 772;

float textWidth (const juce::Font& f, const juce::String& t)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void styleLabel (juce::Label& l, float size, juce::Colour c, bool bold = false, juce::Justification j = juce::Justification::centredLeft, bool kern = false)
{
    l.setFont (kern ? ui::sans (size, bold ? "Bold" : "Medium", 0.14f) : ui::sans (size, bold ? "Bold" : "Regular"));
    l.setColour (juce::Label::textColourId, c);
    l.setJustificationType (j);
    l.setInterceptsMouseClicks (false, false);
}

// Popup opened from an FX toggle: a Mix slider plus any number of extra controls (timing choices, rate...).
struct FxPopup : public juce::Component
{
    struct Extra { const char* id; juce::String name; bool choice; };

    FxPopup (juce::AudioProcessorValueTreeState& apvts, const juce::String& title, const char* mixId, std::vector<Extra> extras = {})
    {
        setLookAndFeel (&laf.get());
        titleLabel.setText (title, juce::dontSendNotification);
        styleLabel (titleLabel, 11.5f, col::text, true, juce::Justification::centredLeft, true);
        addAndMakeVisible (titleLabel);
        rows.push_back (std::make_unique<Row> (apvts, "MIX", mixId, false, *this));
        for (auto& e : extras)
            rows.push_back (std::make_unique<Row> (apvts, e.name.toUpperCase(), e.id, e.choice, *this));
        setSize (310, 40 + (int) rows.size() * 34 + 8);
    }
    ~FxPopup() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics& g) override { g.fillAll (col::panel); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        titleLabel.setBounds (r.removeFromTop (22));
        for (auto& row : rows)
        {
            auto line = r.removeFromTop (34);
            row->label.setBounds (line.removeFromLeft (70));
            row->slider.setBounds (line);
            row->combo.setBounds (line.reduced (0, 3));
        }
    }

    struct Row
    {
        Row (juce::AudioProcessorValueTreeState& apvts, const juce::String& text, const char* id, bool isChoice, juce::Component& parent)
        {
            label.setText (text, juce::dontSendNotification);
            styleLabel (label, 10.5f, col::dim, true, juce::Justification::centredLeft, true);
            parent.addAndMakeVisible (label);
            if (isChoice)
            {
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
                    combo.addItemList (c->choices, 1);
                parent.addAndMakeVisible (combo);
                comboAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, combo);
            }
            else
            {
                slider.setSliderStyle (juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
                parent.addAndMakeVisible (slider);
                sliderAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
            }
        }
        juce::Label label;
        juce::Slider slider;
        juce::ComboBox combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttach;
    };

    juce::SharedResourcePointer<ui::JmcLookAndFeel> laf;
    juce::Label titleLabel;
    std::vector<std::unique_ptr<Row>> rows;
};
} // namespace

// ---------------------------------------------------------------- footer helpers
void JMCReverseEditor::DragZone::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool ok = isReady && isReady();
    g.setColour (col::panel);
    g.fillRect (r);
    juce::Path p, dashed;
    p.addRectangle (r);
    const float dashes[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.4f).createDashedStroke (dashed, p, dashes, 2);
    g.setColour (ok ? col::red : col::line);
    g.fillPath (dashed);
    g.setColour (ok ? col::text : col::faint);
    g.setFont (ui::sans (11.0f, "Bold", 0.14f));
    g.drawText (ok ? juce::String ("DRAG & DROP INTO DAW") : idleTitle, getLocalBounds().withTrimmedBottom (14), juce::Justification::centred);
    g.setColour (col::faint);
    g.setFont (ui::mono (9.5f, false, 0.06f));
    g.drawText (ok ? juce::String ("drag the finished audio onto the arrangement") : idleHint, getLocalBounds().withTrimmedTop (30), juce::Justification::centred);
}

void JMCReverseEditor::DragZone::mouseDrag (const juce::MouseEvent& e)
{
    if (started || e.getDistanceFromDragStart() < 6)
        return;
    started = true;
    if (isReady && ! isReady())
    {
        if (onError) onError ("The effect is still rendering. Wait for \"Ready\", then drag.");
        return;
    }
    juce::String err;
    const auto f = makeFile ? makeFile (err) : juce::File();
    if (! f.existsAsFile())
    {
        if (onError) onError (err.isEmpty() ? "Nothing to drag yet." : err);
        return;
    }
    juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, this, nullptr);
}

void JMCReverseEditor::PeakMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRect (r);
    g.setColour (col::line);
    g.drawRect (r, 1.0f);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float w = r.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour (db > -1.0f ? col::redHi : col::red);
    g.fillRect (r.withWidth (w));
    g.setColour (col::dim);
    g.setFont (ui::mono (9.5f, false, 0.06f));
    g.drawText (level > 0.0001f ? juce::String (db, 1) + " dB" : juce::String ("preview level"), getLocalBounds().reduced (6, 0), juce::Justification::centredRight);
}

// ---------------------------------------------------------------- construction
JMCReverseEditor::JMCReverseEditor (JMCReverseProcessor& p) : AudioProcessorEditor (&p), proc (p), presets (p)
{
    setLookAndFeel (&laf.get());
    namespace I = jmc::ids;

    // ---- header
    for (auto* c : std::initializer_list<juce::Component*> { &presetButton, &prevButton, &nextButton, &saveButton, &undoButton, &redoButton,
                                                              &loadButton, &menuButton, &searchBox })
        addAndMakeVisible (*c);
    searchBox.setTextToShowWhenEmpty ("Search presets", col::faint);
    searchBox.setFont (ui::mono (11.0f, false, 0.06f));
    searchBox.onReturnKey = [this] { showPresetMenu(); };
    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    saveButton.onClick = [this] { savePresetDialog(); };
    undoButton.onClick = [this] { proc.undo(); };
    redoButton.onClick = [this] { proc.redo(); };
    loadButton.onClick = [this] { chooseFile(); };
    menuButton.onClick = [this] { showSettingsMenu(); };
    menuButton.setTooltip ("Settings");
    presetButton.setTooltip ("Choose a preset");
    saveButton.setTooltip ("Save the current settings as a user preset");
    loadButton.setTooltip ("Load a WAV/AIFF file (or drop it on the window)");

    // ---- display
    addAndMakeVisible (sourceView);
    addAndMakeVisible (resultView);
    sourceView.setTitle ("SOURCE");
    resultView.setTitle ("RESULT");
    sourceView.setEmptyText ("Drop an audio file or a clip from your DAW here, or capture from the track");
    resultView.setEmptyText ("The generated reverse reverb appears here");
    resultView.setWaveColour (col::accent);
    resultView.setDisplayGamma (0.5f); // the swell is very quiet at first: lift it for display only
    sourceView.enableTrim (true);
    // Click a waveform to play it from there (SOURCE = the original, RESULT = the generated effect, exactly what is exported).
    sourceView.onSeek = [this] (double pos01) { proc.playFrom (false, pos01); };
    resultView.onSeek = [this] (double pos01) { proc.playFrom (true, pos01); };
    sourceView.onStop = [this] { proc.stop(); };
    resultView.onStop = [this] { proc.stop(); };
    sourceView.setTooltip ("Drop any audio file (or a clip dragged from your DAW) here to use it as the source. Click to play the source from here, double-click to stop. Drag the red handles to trim; Shift-drag adds a sibilance region, Alt-click removes one.");
    resultView.setTooltip ("Click to play the result from here, double-click to stop. Click and drag it into your DAW to place the audio.");
    resultView.dragOutEnabled = true;
    resultView.onDragOut = [this] { startResultDrag(); };
    sourceView.onTrim = [this] (double s, double e, bool b, bool en) { applyTrim (s, e, b, en); };
    sourceView.onAddRegion = [this] (double a, double b) { proc.addSibRegion ((float) a, (float) b); };
    sourceView.onRemoveRegion = [this] (double pos) { proc.removeSibRegionAt ((float) pos); };
    addAndMakeVisible (infoLabel);
    styleLabel (infoLabel, 11.0f, col::mid);
    infoLabel.setFont (ui::mono (10.5f, false, 0.06f));
    addAndMakeVisible (placeLabel);
    styleLabel (placeLabel, 10.0f, col::faint, false, juce::Justification::centredLeft);
    placeLabel.setFont (ui::mono (9.0f, false, 0.04f));

    for (auto* b : { &captureButton, &regenButton, &resetButton, &advButton })
        addAndMakeVisible (*b);
    addAndMakeVisible (loopToggle);
    captureButton.onClick = [this] { if (proc.isCapturing()) proc.stopCapture(); else proc.startCapture(); };
    captureButton.setTooltip ("Record the audio playing through this plugin (insert it on the vocal track), then press Stop");
    loopToggle.setToggleState (proc.isLooping(), juce::dontSendNotification);
    loopToggle.onClick = [this] { proc.setLoop (loopToggle.getToggleState()); };
    regenButton.onClick = [this] { proc.requestRender(); };
    regenButton.setTooltip ("Render again with the current settings");
    resetButton.onClick = [this] { proc.resetAll(); };
    resetButton.setTooltip ("Reset every parameter to its default");
    advButton.onClick = [this] { setAdvanced (! advanced); };
    advButton.setTooltip ("Show or hide the advanced controls");

    // ---- main controls
    addAndMakeVisible (lengthSlider);
    lengthSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    lengthSlider.setColour (juce::Slider::rotarySliderFillColourId, col::ice);
    lengthSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    lengthSlider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (lengthCaption);
    lengthCaption.setText ("LENGTH", juce::dontSendNotification);
    styleLabel (lengthCaption, 12.0f, col::dim, true, juce::Justification::centredRight, true);
    addAndMakeVisible (lengthReadout);
    styleLabel (lengthReadout, 21.0f, col::strong, true, juce::Justification::centredLeft, true);
    addAndMakeVisible (timingLabel);
    styleLabel (timingLabel, 10.0f, col::faint, false, juce::Justification::centredRight);
    timingLabel.setFont (ui::mono (9.5f, false, 0.04f));
    addAndMakeVisible (syncToggle);
    syncAttach = std::make_unique<Apvts::ButtonAttachment> (proc.apvts, I::sync, syncToggle);
    syncToggle.setTooltip ("Lock the length to the host tempo (bars / beats)");
    retargetLength (*proc.apvts.getRawParameterValue (I::sync) > 0.5f);

    kDecay = std::make_unique<ui::Knob> (proc.apvts, I::decay, "REVERB", "Reverb decay (RT60). It is limited to 0.25x-3x the length so the stretch stays natural", col::ice);
    kTilt  = std::make_unique<ui::Knob> (proc.apvts, I::tilt, "TONE", "Tilt: negative = darker, positive = brighter", col::ice);
    kWidth = std::make_unique<ui::Knob> (proc.apvts, I::width, "WIDTH", "Stereo width. Widening is limited so the mono sum never loses more than 6 dB", col::accent, 1.0);
    kDry   = std::make_unique<ui::Knob> (proc.apvts, I::dry, "DRY", "Level of the original source appended after the swell (0 = swell only)");
    kSibSens = std::make_unique<ui::Knob> (proc.apvts, I::sibsens, "SENSITIVITY", "How readily sibilants are detected");
    kSibAmt  = std::make_unique<ui::Knob> (proc.apvts, I::sibamt, "AMOUNT", "How strongly the detected sibilants drive the swell");
    for (auto* k : { kDecay.get(), kTilt.get(), kWidth.get(), kDry.get(), kSibSens.get(), kSibAmt.get() })
        addAndMakeVisible (*k);
    addAndMakeVisible (sibToggle);
    sibAttach = std::make_unique<Apvts::ButtonAttachment> (proc.apvts, I::sibon, sibToggle);
    sibToggle.setTooltip ("Find S / SH sounds and build the swell from them");
    addAndMakeVisible (sibResetButton);
    sibResetButton.onClick = [this] { proc.clearSibEdits(); };
    sibResetButton.setTooltip ("Remove manual additions/removals. Shift-drag on the source waveform adds a region; Alt-click removes one.");
    addAndMakeVisible (sibLabel);
    styleLabel (sibLabel, 11.0f, col::dim, false, juce::Justification::topLeft);
    sibLabel.setFont (ui::mono (9.5f, false, 0.03f));

    // ---- characters + FX + mono check
    for (int i = 0; i < jmc::characters::count; ++i)
    {
        auto& b = charButtons[i];
        b.setButtonText (jmc::characters::names[i]);
        b.onClick = [this, i] { proc.applyCharacter (i + 1); };
        addAndMakeVisible (b);
    }
    {
        static const char* ids3[3] = { I::chorus, I::rampon, I::tremolo };
        static const char* names3[3] = { "CHORUS", "TREM RAMP", "TREMOLO" };
        for (int i = 0; i < 3; ++i)
        {
            fxToggle[i].setButtonText (names3[i]);
            fxMore[i].setButtonText ("MIX");
            fxMore[i].setTooltip ("Mix and timing");
            addAndMakeVisible (fxToggle[i]);
            addAndMakeVisible (fxMore[i]);
            fxAttach[i] = std::make_unique<Apvts::ButtonAttachment> (proc.apvts, ids3[i], fxToggle[i]);
            fxMore[i].onClick = [this, i] { showFxPopup (i); };
        }
    }
    addAndMakeVisible (monoLabel);
    styleLabel (monoLabel, 11.0f, col::mid, false, juce::Justification::topLeft);
    monoLabel.setFont (ui::mono (10.5f, false, 0.05f));

    // ---- advanced drawer
    addAdvKnob (I::size, "SIZE", "Room size (scales the reverb delay lines)", 0);
    addAdvKnob (I::predelay, "PRE-DELAY", "Gap between the end of the swell and the source", 0);
    addAdvKnob (I::damping, "DAMPING", "High-frequency damping inside the reverb", 0);
    addAdvKnob (I::lowdamp, "LOW DAMP", "Extra low-frequency decay inside the reverb", 0);
    addAdvKnob (I::lowcut, "LOW CUT", "High-pass filter before the reverb", 1);
    addAdvKnob (I::highcut, "HIGH CUT", "Low-pass filter before the reverb", 1);
    addAdvKnob (I::balance, "BALANCE", "Stereo balance", 1);
    addAdvKnob (I::outgain, "OUT GAIN", "Final gain (peaks are scaled, never clipped)", 1);
    addAdvKnob (I::fadein, "FADE IN", "Attack of the swell", 2);
    addAdvKnob (I::fadeout, "FADE OUT", "Release at the end of the swell", 2);
    addAdvKnob (I::preroll, "PRE-ROLL", "Silence before the swell starts", 2);
    addAdvKnob (I::tail, "TAIL", "Forward reverb tail after the source onset", 2);
    addAdvKnob (I::pitch, "PITCH", "Pitch movement: starts this far away and arrives at the original pitch at the downbeat", 3);
    addAdvKnob (I::drive, "SATURATION", "Soft saturation", 3);
    addAdvKnob (I::gatedepth, "STUTTER DEPTH", "Depth of the rhythmic stutter (set the rate below)", 3);
    addAdvKnob (I::gateramp, "STUTTER RAMP", "Makes the stutter accelerate towards the downbeat", 3);
    advGroups = { { "REVERB", {} }, { "TONE & OUTPUT", {} }, { "ENVELOPE & TIMING", {} }, { "CREATIVE", {} } };
    for (auto* c : std::initializer_list<juce::Component*> { &curveBox, &gateBox, &modeBox, &curveLabel, &gateLabel, &modeLabel, &autoLevelToggle })
        addChildComponent (*c);
    curveLabel.setText ("FADE CURVE", juce::dontSendNotification);
    gateLabel.setText ("STUTTER RATE", juce::dontSendNotification);
    modeLabel.setText ("SWELL MODE", juce::dontSendNotification);
    styleLabel (modeLabel, 10.5f, col::dim, true, juce::Justification::centredLeft, true);
    modeBox.setTooltip ("Phrase: classic reverse reverb of the whole source (carries the whole phrase's colour). Smooth tail: one continuous swell from the reverb tail after the source (smoothest, colour of the end of the phrase).");
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (I::mode))) modeBox.addItemList (c->choices, 1);
    modeAttach = std::make_unique<Apvts::ComboBoxAttachment> (proc.apvts, I::mode, modeBox);
    styleLabel (curveLabel, 10.5f, col::dim, true, juce::Justification::centredLeft, true);
    styleLabel (gateLabel, 10.5f, col::dim, true, juce::Justification::centredLeft, true);
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (I::curve))) curveBox.addItemList (c->choices, 1);
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (I::gaterate))) gateBox.addItemList (c->choices, 1);
    curveAttach = std::make_unique<Apvts::ComboBoxAttachment> (proc.apvts, I::curve, curveBox);
    gateAttach = std::make_unique<Apvts::ComboBoxAttachment> (proc.apvts, I::gaterate, gateBox);
    autoLevelAttach = std::make_unique<Apvts::ButtonAttachment> (proc.apvts, I::autolevel, autoLevelToggle);
    autoLevelToggle.setTooltip ("Match the swell's peak level to the source (it only ever scales the swell)");

    // ---- footer
    for (auto* c : std::initializer_list<juce::Component*> { &exportButton, &revealButton, &bitsBox, &rateBox, &monoToggle, &exportLabel, &dragZone, &meter })
        addAndMakeVisible (*c);
    bitsBox.addItem ("24-bit", 1);
    bitsBox.addItem ("32-bit float", 2);
    rateBox.addItem ("Rate: as project", 1);
    for (double r : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        rateBox.addItem (juce::String (r / 1000.0, r == 44100.0 || r == 88200.0 ? 1 : 0) + " kHz", (int) (r / 100.0));
    readExportUi();
    bitsBox.onChange = [this] { applyExportUi(); };
    rateBox.onChange = [this] { applyExportUi(); };
    monoToggle.onClick = [this] { applyExportUi(); };
    exportButton.onClick = [this] { exportClicked(); };
    exportButton.setTooltip ("Save the rendered effect (exactly what FX plays) as a WAV file");
    revealButton.onClick = [this] { if (auto f = proc.getLastExportFile(); f.exists()) f.revealToUser(); };
    revealButton.setTooltip ("Show the last exported file in Finder");
    styleLabel (exportLabel, 11.0f, col::mid);
    exportLabel.setFont (ui::mono (10.0f, false, 0.05f));
    dragZone.isReady = [this] { return proc.isResultCurrent(); };
    dragZone.makeFile = [this] (juce::String& err) { return proc.writeDragFile (err); };
    dragZone.onError = [this] (const juce::String& m) { exportLabel.setText (m, juce::dontSendNotification); };
    dragZone.setTooltip ("Drag the finished audio straight into the arrangement");
    for (auto& f : JMCReverseProcessor::dragDir().findChildFiles (juce::File::findFiles, false, "*.wav")) // stale drag files
        if (f.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::hours (2))
            f.deleteFile();

    advanced = (bool) proc.apvts.state.getProperty ("uiAdvanced", false);
    setAdvanced (advanced);
    startTimerHz (30);
    timerCallback();
}

JMCReverseEditor::~JMCReverseEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void JMCReverseEditor::addAdvKnob (const char* id, const char* caption, const char* tip, int group)
{
    namespace I = jmc::ids;
    const std::string sid (id);
    const bool timeFreq = sid == I::predelay || sid == I::lowcut || sid == I::highcut || sid == I::fadein || sid == I::fadeout
                          || sid == I::preroll || sid == I::tail || sid == I::pitch;
    auto k = std::make_unique<ui::Knob> (proc.apvts, id, caption, tip, timeFreq ? col::ice : col::accent);
    addChildComponent (*k);
    advKnobs.push_back (std::move (k));
    advKnobGroup.push_back (group);
}

void JMCReverseEditor::setAdvanced (bool on)
{
    advanced = on;
    proc.apvts.state.setProperty ("uiAdvanced", on, nullptr);
    for (auto& k : advKnobs) k->setVisible (on);
    for (auto* c : std::initializer_list<juce::Component*> { &curveBox, &gateBox, &modeBox, &curveLabel, &gateLabel, &modeLabel, &autoLevelToggle })
        c->setVisible (on);
    advButton.setButtonText (on ? "ADVANCED  <" : "ADVANCED  >");
    setSize (kWinW + (on ? kAdvW : 0), kWinH);
}

// ---------------------------------------------------------------- painting + layout
void JMCReverseEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::line);
    g.drawHorizontalLine (59, 0.0f, (float) getWidth());

    // Wordmark, same construction as JMC VERB: JMC heavy + expanded, product name lighter in the accent.
    {
        auto jmc = ui::sans (27.0f, "Bold", -0.02f);
        jmc.setHorizontalScale (1.14f);
        auto name = ui::sans (27.0f, "Medium", 0.01f);
        name.setHorizontalScale (1.14f);
        const float jw = textWidth (jmc, "JMC");
        g.setColour (col::strong);
        g.setFont (jmc);
        g.drawText ("JMC", juce::Rectangle<float> (24.0f, 14.0f, jw + 6.0f, 32.0f), juce::Justification::centredLeft, false);
        g.setColour (col::accent);
        g.setFont (name);
        g.drawText ("REVERSE", juce::Rectangle<float> (24.0f + jw, 14.0f, 150.0f, 32.0f), juce::Justification::centredLeft, false);
    }

    auto panelOutline = [&] (juce::Rectangle<int> r)
    {
        g.setColour (col::footer);
        g.fillRect (r);
        g.setColour (col::line);
        g.drawRect (r, 1);
    };
    for (auto r : { displayPanel, mainPanel, charPanel, footerPanel })
        panelOutline (r);
    if (advanced) panelOutline (advPanel);

    g.setColour (col::line);
    for (int x : dividerX)
        g.drawVerticalLine (x, (float) mainPanel.getY() + 16.0f, (float) mainPanel.getBottom() - 16.0f);

    // section captions: accent index + tracked label, like the numbered sections of JMC VERB
    auto title = [&] (const juce::String& num, const juce::String& t, int x, int y)
    {
        g.setColour (col::accent);
        g.setFont (ui::mono (10.5f));
        g.drawText (num, x, y, 22, 14, juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (ui::sans (10.0f, "Bold", 0.14f));
        g.drawText (t, x + 24, y, 200, 14, juce::Justification::centredLeft);
    };
    title ("04", "CHARACTER", charPanel.getX() + 14, charPanel.getY() + 8);
    title ("05", "FX", charPanel.getX() + 14 + 560 + 24, charPanel.getY() + 8);
    title ("06", "STEREO / MONO CHECK", charPanel.getX() + 14 + 560 + 24 + 190, charPanel.getY() + 8);
    if (advanced)
    {
        title ("07", "ADVANCED", advPanel.getX() + 16, advPanel.getY() + 10);
        for (auto& gr : advGroups)
        {
            g.setColour (col::dim);
            g.setFont (ui::sans (10.0f, "Bold", 0.14f));
            g.drawText (gr.title, gr.titleBounds, juce::Justification::centredLeft);
            g.setColour (col::line);
            g.drawHorizontalLine (gr.titleBounds.getBottom() + 1, (float) gr.titleBounds.getX(), (float) gr.titleBounds.getRight());
        }
    }

    // state chip
    const auto s = proc.getState();
    static const char* names[] = { "EMPTY", "LOADING", "ANALYZING", "RENDERING", "READY", "ERROR" };
    const juce::Colour dot = s == EngineState::Ready ? col::ok : (s == EngineState::Empty ? col::faint : col::accent);
    g.setColour (dot);
    g.fillEllipse ((float) chipArea.getX(), (float) chipArea.getCentreY() - 3.5f, 7.0f, 7.0f);
    g.setColour (s == EngineState::Ready ? col::mid : (s == EngineState::Error ? col::accent : col::dim));
    g.setFont (ui::sans (10.0f, "Bold", 0.14f));
    g.drawText (names[(int) s], chipArea.withTrimmedLeft (15), juce::Justification::centredLeft);
}

void JMCReverseEditor::resized()
{
    const int m = 24;
    headerArea = { 0, 0, getWidth(), 60 };

    // header controls
    {
        const int y = 16, h = 28;
        prevButton.setBounds (240, y, 28, h);
        presetButton.setBounds (270, y, 176, h);
        nextButton.setBounds (448, y, 28, h);
        searchBox.setBounds (486, y, 104, h);
        saveButton.setBounds (598, y, 60, h);
        undoButton.setBounds (664, y, 50, h);
        redoButton.setBounds (718, y, 50, h);
        loadButton.setBounds (kWinW - m - 42 - 6 - 104, y, 104, h);
        menuButton.setBounds (kWinW - m - 42, y, 42, h);
    }

    const int pw = kWinW - 2 * m;
    int y = 72;

    // display panel
    displayPanel = { m, y, pw, 244 };
    {
        auto r = displayPanel.reduced (12, 8);
        auto top = r.removeFromTop (20);
        chipArea = top.removeFromRight (110);
        infoLabel.setBounds (top);
        r.removeFromTop (2);
        sourceView.setBounds (r.removeFromTop (84));
        r.removeFromTop (4);
        resultView.setBounds (r.removeFromTop (84));
        r.removeFromTop (6);
        auto bar = r.removeFromTop (30);
        captureButton.setBounds (bar.removeFromLeft (96));
        bar.removeFromLeft (8);
        loopToggle.setBounds (bar.removeFromLeft (70));
        bar.removeFromLeft (10);
        regenButton.setBounds (bar.removeFromLeft (108));
        advButton.setBounds (bar.removeFromRight (120));
        bar.removeFromRight (6);
        resetButton.setBounds (bar.removeFromRight (70));
        bar.removeFromRight (10);
        placeLabel.setBounds (bar);
    }
    y = displayPanel.getBottom() + 8;

    // main controls panel
    mainPanel = { m, y, pw, 204 };
    dividerX.clear();
    {
        auto r = mainPanel.reduced (14, 10);
        auto big = r.removeFromLeft (262);
        lengthSlider.setBounds (big.getCentreX() - 70, big.getY(), 140, 140);
        auto ro = big.withTrimmedTop (142).removeFromTop (30);
        lengthCaption.setBounds (ro.removeFromLeft (big.getWidth() / 2 - 8));
        ro.removeFromLeft (14);
        lengthReadout.setBounds (ro);
        auto sub = big.withTrimmedTop (176).removeFromTop (20);
        syncToggle.setBounds (sub.removeFromLeft (136));
        timingLabel.setBounds (sub);
        dividerX.add (r.getX() + 6);
        r.removeFromLeft (14);

        const int kw = 106;
        auto knobs = r.removeFromLeft (kw * 4);
        auto kr = knobs.withSizeKeepingCentre (kw * 4, 118);
        for (auto* k : { kDecay.get(), kTilt.get(), kWidth.get(), kDry.get() })
            k->setBounds (kr.removeFromLeft (kw));
        dividerX.add (r.getX() + 6);
        r.removeFromLeft (14);

        auto sib = r;
        sibToggle.setBounds (sib.removeFromTop (22));
        sib.removeFromTop (6);
        auto sk = sib.removeFromTop (100);
        kSibSens->setBounds (sk.removeFromLeft (sk.getWidth() / 2));
        kSibAmt->setBounds (sk);
        sib.removeFromTop (4);
        sibLabel.setBounds (sib.removeFromTop (34));
        sibResetButton.setBounds (sib.removeFromTop (24).removeFromLeft (110));
    }
    y = mainPanel.getBottom() + 8;

    // characters / FX / mono check
    charPanel = { m, y, pw, 122 };
    {
        auto r = charPanel.reduced (14, 8);
        r.removeFromTop (18);
        auto grid = r.removeFromLeft (560);
        const int bw = (560 - 3 * 6) / 4, bh = 25;
        for (int i = 0; i < jmc::characters::count; ++i)
            charButtons[i].setBounds (grid.getX() + (i % 4) * (bw + 6), grid.getY() + (i / 4) * (bh + 5), bw, bh);
        r.removeFromLeft (24);
        auto fx = r.removeFromLeft (166);
        for (int i = 0; i < 3; ++i)
        {
            auto row = fx.removeFromTop (bh);
            fxToggle[i].setBounds (row.removeFromLeft (118));
            fxMore[i].setBounds (row.removeFromRight (46));
            fx.removeFromTop (5);
        }
        r.removeFromLeft (24);
        monoLabel.setBounds (r);
    }
    y = charPanel.getBottom() + 8;

    // footer
    footerPanel = { m, y, pw, 96 };
    {
        auto r = footerPanel.reduced (14, 10);
        dragZone.setBounds (r.removeFromRight (360));
        r.removeFromRight (16);
        auto row1 = r.removeFromTop (28);
        exportButton.setBounds (row1.removeFromLeft (112));
        row1.removeFromLeft (6);
        bitsBox.setBounds (row1.removeFromLeft (108));
        row1.removeFromLeft (6);
        rateBox.setBounds (row1.removeFromLeft (128));
        row1.removeFromLeft (10);
        monoToggle.setBounds (row1.removeFromLeft (84));
        revealButton.setBounds (row1.removeFromLeft (72));
        r.removeFromTop (6);
        exportLabel.setBounds (r.removeFromTop (20));
        r.removeFromTop (4);
        meter.setBounds (r.removeFromTop (16));
    }

    // advanced drawer
    if (advanced)
    {
        advPanel = { kWinW, 72, kAdvW - m, kWinH - 72 - 8 };
        auto r = advPanel.reduced (16, 34);
        const int cw = r.getWidth() / 4, ch = 92;
        int cy = r.getY();
        size_t ki = 0;
        for (int gi = 0; gi < 4; ++gi)
        {
            advGroups[(size_t) gi].titleBounds = { r.getX(), cy, r.getWidth(), 16 };
            cy += 22;
            int col4 = 0;
            while (ki < advKnobs.size() && advKnobGroup[ki] == gi)
            {
                advKnobs[ki]->setBounds (r.getX() + col4 * cw, cy, cw, ch);
                ++ki; ++col4;
            }
            cy += ch + 2;
            if (gi == 0)
            {
                modeLabel.setBounds (r.getX() + 2, cy, 100, 26);
                modeBox.setBounds (r.getX() + 106, cy, 150, 26);
                cy += 32;
            }
            else if (gi == 1)
            {
                autoLevelToggle.setBounds (r.getX() + 2, cy, 130, 24);
                cy += 30;
            }
            else if (gi == 2)
            {
                curveLabel.setBounds (r.getX() + 2, cy, 90, 26);
                curveBox.setBounds (r.getX() + 96, cy, 150, 26);
                cy += 32;
            }
            else if (gi == 3)
            {
                gateLabel.setBounds (r.getX() + 2, cy, 100, 26);
                gateBox.setBounds (r.getX() + 106, cy, 110, 26);
                cy += 32;
            }
            cy += 6;
        }
    }
    else
        advPanel = {};
}

// ---------------------------------------------------------------- actions
juce::String JMCReverseEditor::currentLabel() const
{
    const int ch = proc.getCharacter();
    juce::String n = proc.getPresetName();
    if (n.isEmpty() && ch >= 1 && ch <= jmc::characters::count)
        n = jmc::characters::names[ch - 1];
    if (n.isEmpty())
        n = "Init";
    return n + (proc.isCharacterModified() ? " *" : "");
}

void JMCReverseEditor::showPresetMenu()
{
    auto list = std::make_shared<std::vector<jmc::PresetInfo>> (presets.list());
    const auto filter = searchBox.getText().trim();
    juce::PopupMenu menu;
    juce::String lastCat;
    bool any = false;
    for (size_t i = 0; i < list->size(); ++i)
    {
        const auto& pi = (*list)[i];
        if (filter.isNotEmpty() && ! pi.name.containsIgnoreCase (filter) && ! pi.category.containsIgnoreCase (filter))
            continue;
        const juce::String cat = (pi.factory ? "FACTORY - " : "USER - ") + pi.category.toUpperCase();
        if (cat != lastCat) { menu.addSectionHeader (cat); lastCat = cat; }
        menu.addItem ((int) i + 1, pi.name, true, pi.name == proc.getPresetName());
        any = true;
    }
    if (! any) menu.addItem (-1, "No presets match", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMaximumNumColumns (1),
                        [this, list] (int id) { if (id > 0) presets.apply ((*list)[(size_t) id - 1]); });
}

void JMCReverseEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("SETTINGS");
    m.addItem (1, "Auto-capture while the host plays", true, proc.getAutoCapture());
    m.addSeparator();
    m.addItem (2, "Show captures folder");
    m.addItem (3, "Show user presets folder");
    m.addSeparator();
    m.addItem (4, juce::String ("JMC REVERSE ") + JucePlugin_VersionString, false, false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [this] (int id)
    {
        if (id == 1) proc.setAutoCapture (! proc.getAutoCapture());
        else if (id == 2) { auto d = JMCReverseProcessor::capturesDir(); d.createDirectory(); d.revealToUser(); }
        else if (id == 3) { auto d = presets.getUserDir(); d.createDirectory(); d.revealToUser(); }
    });
}

void JMCReverseEditor::stepPreset (int delta)
{
    auto list = presets.list();
    if (list.empty()) return;
    int cur = -1;
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].name == proc.getPresetName()) cur = (int) i;
    const int next = cur < 0 ? (delta > 0 ? 0 : (int) list.size() - 1) : (cur + delta + (int) list.size()) % (int) list.size();
    presets.apply (list[(size_t) next]);
}

void JMCReverseEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset (saved to your user preset folder).", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.getPresetName(), "Name:");
    w->addComboBox ("cat", juce::StringArray { "Vocals", "Ad-libs", "Transitions", "Sound Design", "User" }, "Category:");
    w->getComboBoxComponent ("cat")->setSelectedItemIndex (4);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            juce::String err;
            if (! presets.saveUser (w->getTextEditorContents ("name"), w->getComboBoxComponent ("cat")->getText(), err))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save preset", err);
        }
        delete w;
    }), false);
}

void JMCReverseEditor::showFxPopup (int which)
{
    namespace I = jmc::ids;
    std::unique_ptr<juce::Component> content;
    if (which == 0)
        content = std::make_unique<FxPopup> (proc.apvts, "CHORUS", I::moddepth, std::vector<FxPopup::Extra> { { I::modrate, "Rate", false } });
    else if (which == 1)
        content = std::make_unique<FxPopup> (proc.apvts, "TREMOLO RAMP   START > MID > END", I::rampmix,
                                             std::vector<FxPopup::Extra> { { I::rampa, "Start", true }, { I::rampb, "Mid", true }, { I::rampc, "End", true } });
    else
        content = std::make_unique<FxPopup> (proc.apvts, "TREMOLO", I::tremmix, std::vector<FxPopup::Extra> { { I::tremrate, "Timing", true } });
    juce::CallOutBox::launchAsynchronously (std::move (content), fxMore[which].getScreenBounds(), nullptr);
}

void JMCReverseEditor::readExportUi()
{
    const auto o = proc.getExportOptions();
    bitsBox.setSelectedId (o.bits == 32 ? 2 : 1, juce::dontSendNotification);
    rateBox.setSelectedId (o.sampleRate > 0 ? (int) (o.sampleRate / 100.0) : 1, juce::dontSendNotification);
    monoToggle.setToggleState (o.mono, juce::dontSendNotification);
}

void JMCReverseEditor::applyExportUi()
{
    jmc::ExportOptions o;
    o.bits = bitsBox.getSelectedId() == 2 ? 32 : 24;
    o.sampleRate = rateBox.getSelectedId() > 1 ? rateBox.getSelectedId() * 100.0 : 0.0;
    o.mono = monoToggle.getToggleState();
    proc.setExportOptions (o);
}

void JMCReverseEditor::startResultDrag()
{
    proc.stop();
    if (! proc.isResultCurrent())
    {
        exportLabel.setText ("The effect is still rendering. Wait for \"Ready\", then drag.", juce::dontSendNotification);
        return;
    }
    juce::String err;
    const auto f = proc.writeDragFile (err);
    if (! f.existsAsFile())
    {
        exportLabel.setText (err.isEmpty() ? juce::String ("Nothing to drag yet.") : err, juce::dontSendNotification);
        return;
    }
    juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, &resultView, nullptr);
}

void JMCReverseEditor::exportClicked()
{
    if (! proc.isResultCurrent())
    {
        exportLabel.setText ("The effect is still rendering. Wait for \"Ready\" and try again.", juce::dontSendNotification);
        return;
    }
    applyExportUi();
    auto start = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (proc.suggestedExportName());
    if (auto last = proc.getLastExportFile(); last != juce::File())
        start = last.getParentDirectory().getChildFile (proc.suggestedExportName());
    saveChooser = std::make_unique<juce::FileChooser> ("Export WAV", start, "*.wav");
    saveChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f != juce::File())
                                      proc.exportResultAsync (f.withFileExtension ("wav"));
                              });
}

void JMCReverseEditor::applyTrim (double s, double e, bool begin, bool end)
{
    auto* ps = proc.apvts.getParameter (jmc::ids::trimstart);
    auto* pe = proc.apvts.getParameter (jmc::ids::trimend);
    if (begin) { ps->beginChangeGesture(); pe->beginChangeGesture(); }
    ps->setValueNotifyingHost ((float) s);
    pe->setValueNotifyingHost ((float) e);
    if (end) { ps->endChangeGesture(); pe->endChangeGesture(); }
}

static bool anySupported (const juce::StringArray& files)
{
    for (auto& f : files)
        if (jmc::hasSupportedExtension (juce::File (f)))
            return true;
    return false;
}

bool JMCReverseEditor::isInterestedInFileDrag (const juce::StringArray& f) { return anySupported (f); }

void JMCReverseEditor::filesDropped (const juce::StringArray& files, int, int)
{
    sourceView.setDropHighlight (false);
    for (auto& f : files)
        if (jmc::hasSupportedExtension (juce::File (f)))
        {
            proc.loadFile (juce::File (f));
            return;
        }
}

void JMCReverseEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.aac;*.caf;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != juce::File())
                                  proc.loadFile (f);
                          });
}

void JMCReverseEditor::retargetLength (bool sync)
{
    lengthAttach.reset();
    divAttach.reset();
    if (sync) divAttach = std::make_unique<Apvts::SliderAttachment> (proc.apvts, jmc::ids::division, lengthSlider);
    else      lengthAttach = std::make_unique<Apvts::SliderAttachment> (proc.apvts, jmc::ids::length, lengthSlider);
    lengthSlider.setTooltip (sync ? "Length in beats / bars, locked to the host tempo" : "Length of the swell in seconds");
    shownSync = sync;
    lengthBound = true;
}

void JMCReverseEditor::updateLengthReadout()
{
    const double bpm = proc.getHostBpm();
    const juce::String bpmTxt = juce::String (bpm, 1) + " BPM" + (proc.hostBpmKnown() ? "" : "*");
    if (shownSync)
    {
        const int idx = juce::jlimit (0, jmc::timing::numDivisions - 1, (int) std::lround (*proc.apvts.getRawParameterValue (jmc::ids::division)));
        const double beats = jmc::timing::divisionBeats[idx];
        const double bars = jmc::timing::beatsToBars (beats, proc.getHostTimeSigNum(), proc.getHostTimeSigDen());
        juce::String t;
        if (bars >= 0.999)
            t = jmc::trimNumber (bars) + (std::abs (bars - 1.0) < 0.01 ? " BAR" : " BARS");
        else if (beats >= 1.0)
            t = jmc::trimNumber (beats) + (beats == 1.0f ? " BEAT" : " BEATS");
        else
            t = juce::String (beats == 0.5f ? "1/2" : "1/4") + " BEAT";
        lengthReadout.setText (t, juce::dontSendNotification);
        timingLabel.setText (juce::String (jmc::timing::beatsToSeconds (beats, bpm), 2) + " s  " + bpmTxt, juce::dontSendNotification);
    }
    else
    {
        lengthReadout.setText (juce::String (*proc.apvts.getRawParameterValue (jmc::ids::length), 2) + " S", juce::dontSendNotification);
        timingLabel.setText ("free  " + bpmTxt, juce::dontSendNotification);
    }
}

// ---------------------------------------------------------------- timer
void JMCReverseEditor::timerCallback()
{
    proc.collectGarbage();
    proc.updateRenderIfNeeded();
    proc.commitUndoIfStable();
    undoButton.setEnabled (proc.canUndo());
    redoButton.setEnabled (proc.canRedo());
    meter.setLevel (proc.takePreviewPeak());

    if (const auto label = currentLabel(); label != shownPresetLabel)
    {
        shownPresetLabel = label;
        presetButton.setButtonText (label);
    }
    if (proc.getExportRevision() != shownExportRev)
    {
        shownExportRev = proc.getExportRevision();
        exportLabel.setText (proc.getExportStatus(), juce::dontSendNotification);
    }
    {
        const bool ready = proc.isResultCurrent();
        exportButton.setEnabled (ready);
        revealButton.setEnabled (proc.getLastExportFile().exists());
        dragZone.repaint();
    }

    if (const bool sync = syncToggle.getToggleState(); sync != shownSync || ! lengthBound)
        retargetLength (sync);
    updateLengthReadout();
    for (auto* k : { kDecay.get(), kTilt.get(), kWidth.get(), kDry.get(), kSibSens.get(), kSibAmt.get() })
        k->refresh();
    if (advanced)
        for (auto& k : advKnobs) k->refresh();

    if (proc.getSourceRevision() != shownSource)
    {
        shownSource = proc.getSourceRevision();
        auto src = proc.getSource();
        sourceView.setBuffer (src != nullptr ? WaveformView::Buffer (src, &src->buffer) : nullptr);
        fileInfo = src == nullptr ? juce::String()
                                  : src->name + "    " + juce::String (src->durationSeconds(), 2) + " s    "
                                        + juce::String (src->sampleRate / 1000.0, 1) + " kHz    "
                                        + (src->numChannels() == 1 ? "mono" : "stereo");
    }
    if (proc.getRenderRevision() != shownRender)
    {
        shownRender = proc.getRenderRevision();
        auto res = proc.getRendered();
        resultView.setBuffer (res != nullptr ? WaveformView::Buffer (res, &res->buffer) : nullptr);
        const int total = res != nullptr ? res->buffer.getNumSamples() : 0;
        const double riser = proc.getRiserSeconds() * (res != nullptr ? res->sampleRate : 1.0);
        resultView.setMarker (total > 0 && riser < total ? riser / total : -1.0);
    }
    sourceView.setTrim (*proc.apvts.getRawParameterValue (jmc::ids::trimstart), *proc.apvts.getRawParameterValue (jmc::ids::trimend));

    const auto s = proc.getState();
    if (s == EngineState::Error)
    {
        sourceView.setMessage (proc.getErrorMessage());
        infoLabel.setText (proc.getErrorMessage(), juce::dontSendNotification);
    }
    else
    {
        sourceView.setMessage ({});
        const auto n = proc.getNote();
        infoLabel.setText (fileInfo + (n.isNotEmpty() ? "    " + n : juce::String()), juce::dontSendNotification);
    }
    {
        const double onset = proc.getRiserSeconds();
        placeLabel.setText (juce::String ("Click a waveform to play it - double-click stops - drag RESULT into the DAW")
                                + (onset > 0 ? "\nMarker = downbeat, " + juce::String (onset, 3) + " s into the file" : juce::String()),
                            juce::dontSendNotification);
    }
    if (s == EngineState::Empty)      { dragZone.idleTitle = "NO AUDIO YET";  dragZone.idleHint = "load or capture audio first"; }
    else if (s == EngineState::Error) { dragZone.idleTitle = "NO RESULT";     dragZone.idleHint = "fix the error shown above"; }
    else                              { dragZone.idleTitle = "RENDERING...";  dragZone.idleHint = "available when the result is Ready"; }
    static EngineState lastState = EngineState::Empty;
    if (s != lastState) { lastState = s; repaint (chipArea.expanded (4)); }

    // buttons that mirror state
    {
        const bool cap = proc.isCapturing();
        captureButton.setButtonText (cap ? "STOP " + juce::String (proc.getCaptureSeconds(), 1) + " s" : "CAPTURE");
        captureButton.setColour (juce::TextButton::buttonColourId, cap ? col::accent : col::panel);
        const int ch = proc.getCharacter();
        const bool mod = proc.isCharacterModified();
        if (ch != shownCharacter || mod != shownModified)
        {
            shownCharacter = ch;
            shownModified = mod;
            for (int i = 0; i < jmc::characters::count; ++i)
            {
                const bool sel = (i + 1 == ch);
                charButtons[i].setColour (juce::TextButton::buttonColourId, sel ? (mod ? col::accent.withAlpha (0.35f) : col::accent) : col::panel);
            }
        }
    }
    {
        const bool on = sibToggle.getToggleState();
        const auto rev = proc.getSibRevision();
        if (rev != shownSib || on != shownSibOn)
        {
            shownSib = rev;
            shownSibOn = on;
            std::vector<std::pair<double, double>> v;
            if (on)
                for (auto& g : proc.getSibRegions())
                    v.emplace_back (g.start, g.end);
            sourceView.setRegions (std::move (v));
            sibLabel.setText (on ? proc.getSibMessage() : juce::String ("Off - the whole source is used."), juce::dontSendNotification);
        }
        sibResetButton.setEnabled (on);
        kSibSens->setEnabled (on);
        kSibAmt->setEnabled (on);
    }
    {
        float corr, loss, width;
        proc.getRenderedStats (corr, loss, width);
        juce::String t = "Correlation  " + juce::String (corr, 2) + "\nMono sum  " + juce::String (loss, 1) + " dB";
        if (width < proc.apvts.getRawParameterValue (jmc::ids::width)->load() - 0.01f)
            t << "\nWidth limited to " << juce::String (width, 2) << " to protect mono";
        monoLabel.setText (t, juce::dontSendNotification);
    }

    const bool res = proc.isPreviewResult();
    const double ph = proc.getPlayheadNormalised();
    sourceView.setPlayhead (ph, ! res && proc.isPlaying());
    resultView.setPlayhead (ph, res && proc.isPlaying());
}

#pragma once
#include "PluginProcessor.h"
#include "Characters.h"
#include "Presets.h"
#include "ui/Knob.h"
#include "ui/LookAndFeel.h"
#include "ui/WaveformView.h"

class JMCReverseEditor : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    explicit JMCReverseEditor (JMCReverseProcessor&);
    ~JMCReverseEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { sourceView.setDropHighlight (true); }
    void fileDragExit (const juce::StringArray&) override { sourceView.setDropHighlight (false); }

private:
    using Apvts = juce::AudioProcessorValueTreeState;

    struct DragZone : public juce::Component, public juce::SettableTooltipClient
    {
        std::function<juce::File (juce::String&)> makeFile;
        std::function<bool()> isReady;
        std::function<void (const juce::String&)> onError;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { started = false; }
        void mouseDrag (const juce::MouseEvent&) override;
        bool started = false;
        juce::String idleTitle { "RENDERING..." }, idleHint { "available when the result is Ready" };
    };
    struct PeakMeter : public juce::Component
    {
        void setLevel (float lin) { level = juce::jmax (lin, level * 0.88f); repaint(); }
        void paint (juce::Graphics&) override;
        float level = 0.0f;
    };

    void timerCallback() override;
    void chooseFile();
    void showPresetMenu();
    void showSettingsMenu();
    void stepPreset (int delta);
    void savePresetDialog();
    void showFxPopup (int which);
    void exportClicked();
    void startResultDrag();
    void readExportUi();
    void applyExportUi();
    void applyTrim (double s, double e, bool begin, bool end);
    void retargetLength (bool sync);
    void setAdvanced (bool on);
    void updateLengthReadout();
    juce::String currentLabel() const;
    void addAdvKnob (const char* id, const char* caption, const char* tip, int group);

    juce::SharedResourcePointer<jmc::ui::JmcLookAndFeel> laf;
    juce::TooltipWindow tooltipWindow { this, 500 };
    JMCReverseProcessor& proc;
    jmc::PresetManager presets;

    // header
    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" },
                     undoButton { "UNDO" }, redoButton { "REDO" }, loadButton { "LOAD AUDIO" }, menuButton { "..." };
    juce::TextEditor searchBox;
    juce::String shownPresetLabel;

    // display + transport
    WaveformView sourceView, resultView;
    juce::Label infoLabel, placeLabel;
    juce::String fileInfo;
    juce::TextButton captureButton { "CAPTURE" },
                     regenButton { "REGENERATE" }, resetButton { "RESET" }, advButton { "ADVANCED" };
    juce::ToggleButton loopToggle { "LOOP" };

    // main controls
    juce::Slider lengthSlider;
    std::unique_ptr<Apvts::SliderAttachment> lengthAttach, divAttach;
    juce::Label lengthCaption, lengthReadout, timingLabel;
    juce::ToggleButton syncToggle { "TEMPO SYNC" };
    std::unique_ptr<Apvts::ButtonAttachment> syncAttach;
    bool shownSync = false, lengthBound = false;
    std::unique_ptr<jmc::ui::Knob> kDecay, kTilt, kWidth, kDry, kSibSens, kSibAmt;
    juce::ToggleButton sibToggle { "SIBILANCE DETECTION" };
    std::unique_ptr<Apvts::ButtonAttachment> sibAttach;
    juce::TextButton sibResetButton { "RESET EDITS" };
    juce::Label sibLabel;
    juce::uint32 shownSib = 0xffffffff;
    bool shownSibOn = false;

    // characters, FX, mono check
    juce::TextButton charButtons[jmc::characters::count];
    int shownCharacter = -1;
    bool shownModified = false;
    juce::ToggleButton fxToggle[3];
    juce::TextButton fxMore[3];
    std::unique_ptr<Apvts::ButtonAttachment> fxAttach[3];
    juce::Label monoLabel;

    // advanced drawer
    struct AdvGroup { juce::String title; juce::Rectangle<int> titleBounds; };
    std::vector<std::unique_ptr<jmc::ui::Knob>> advKnobs;
    std::vector<int> advKnobGroup;
    std::vector<AdvGroup> advGroups;
    juce::ComboBox curveBox, gateBox, modeBox;
    juce::Label curveLabel, gateLabel, modeLabel;
    std::unique_ptr<Apvts::ComboBoxAttachment> curveAttach, gateAttach, modeAttach;
    juce::ToggleButton autoLevelToggle { "AUTO LEVEL" };
    std::unique_ptr<Apvts::ButtonAttachment> autoLevelAttach;
    bool advanced = false;

    // footer
    juce::TextButton exportButton { "EXPORT WAV" }, revealButton { "REVEAL" };
    juce::ComboBox bitsBox, rateBox;
    juce::ToggleButton monoToggle { "MONO" };
    juce::Label exportLabel;
    DragZone dragZone;
    PeakMeter meter;
    std::unique_ptr<juce::FileChooser> chooser, saveChooser;
    juce::uint32 shownSource = 0xffffffff, shownRender = 0xffffffff, shownExportRev = 0xffffffff;

    // layout cache for painting
    juce::Rectangle<int> headerArea, displayPanel, mainPanel, charPanel, footerPanel, advPanel, chipArea;
    juce::Array<int> dividerX;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JMCReverseEditor)
};

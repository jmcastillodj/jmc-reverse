#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include <cmath>

namespace jmc::ui
{
// Rotary control bound to a parameter, with a caption and a live value read-out underneath.
// Double-click resets to the parameter default (set by the attachment).
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& captionText, const juce::String& tip = {},
          juce::Colour accent = col::accent, double zeroValue = std::nan (""))
        : param (apvts.getParameter (paramId))
    {
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        if (! std::isnan (zeroValue))
            slider.getProperties().set ("zeroValue", zeroValue);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        slider.setTooltip (tip.isEmpty() ? captionText : tip);
        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
        addAndMakeVisible (slider);
        caption.setText (captionText, juce::dontSendNotification);
        caption.setFont (sans (10.0f, "Bold", 0.14f));
        caption.setColour (juce::Label::textColourId, col::dim);
        caption.setJustificationType (juce::Justification::centred);
        caption.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (caption);
        value.setFont (mono (11.0f, false, 0.08f));
        value.setColour (juce::Label::textColourId, col::text);
        value.setJustificationType (juce::Justification::centred);
        value.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (value);
        slider.onValueChange = [this] { refresh(); };
        refresh();
    }

    void refresh()
    {
        if (param == nullptr) return;
        auto t = param->getText (param->getValue(), 0);
        if (auto u = param->getLabel(); u.isNotEmpty()) t << " " << u;
        if (t != value.getText()) value.setText (t, juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        value.setBounds (r.removeFromBottom (16));
        caption.setBounds (r.removeFromBottom (14));
        slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())));
    }

    juce::Slider slider;

private:
    juce::RangedAudioParameter* param;
    juce::Label caption, value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
};
} // namespace jmc::ui

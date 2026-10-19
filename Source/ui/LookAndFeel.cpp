#include "LookAndFeel.h"

namespace jmc::ui
{
JmcLookAndFeel::JmcLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::strong);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::Label::textColourId, col::dim);
    setColour (juce::TextEditor::backgroundColourId, col::panel);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::border);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::TextButton::buttonColourId, col::panel);
    setColour (juce::TextButton::textColourOffId, col::mid);
    setColour (juce::TextButton::textColourOnId, col::strong);
    setColour (juce::TooltipWindow::backgroundColourId, col::panel2);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TooltipWindow::outlineColourId, col::border);
    setColour (juce::AlertWindow::backgroundColourId, col::bg);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::border);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxBackgroundColourId, col::panel);
    setColour (juce::Slider::textBoxOutlineColourId, col::border);
    setColour (juce::Slider::trackColourId, col::accent);
    setColour (juce::Slider::backgroundColourId, col::track);
    setColour (juce::Slider::thumbColourId, col::strong);
    setColour (juce::Slider::rotarySliderFillColourId, col::accent);
    setColour (juce::ScrollBar::thumbColourId, col::faint);
}

// Same construction as JMC VERB's knob: 100-unit design, track ring, value arc, graphite body, needle.
void JmcLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const juce::Rectangle<float> b ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float k = juce::jmin (b.getWidth(), b.getHeight()) * 0.01f;
    const float angle = start + pos * (end - start);
    const float alpha = s.isEnabled() ? 1.0f : 0.4f;
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId).withMultipliedAlpha (alpha);
    const float sw = juce::jmax (2.2f, 5.5f * k);

    const float tr = 43.0f * k;
    juce::Path track;
    track.addCentredArc (c.x, c.y, tr, tr, 0.0f, start, end, true);
    g.setColour (col::track.withMultipliedAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc from the minimum, or from the zero mark when the control is bipolar
    float from = start;
    bool hasZero = false;
    if (s.getProperties().contains ("zeroValue"))
    {
        from = start + (float) s.valueToProportionOfLength ((double) s.getProperties()["zeroValue"]) * (end - start);
        hasZero = true;
    }
    else if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
    {
        from = start + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (end - start);
        hasZero = true;
    }
    g.setColour (accent);
    if (std::abs (angle - from) > 0.012f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, tr, tr, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.strokePath (val, juce::PathStrokeType (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        const auto p = c.getPointOnCircumference (tr, angle);
        g.fillEllipse (p.x - sw * 0.5f, p.y - sw * 0.5f, sw, sw);
    }
    if (hasZero)
    {
        g.setColour (col::strong.withAlpha (0.9f * alpha));
        g.drawLine ({ c.getPointOnCircumference (39.0f * k, from), c.getPointOnCircumference (47.0f * k, from) }, juce::jmax (1.2f, 1.6f * k));
    }

    g.setColour (col::panel2);
    g.fillEllipse (c.x - 32.0f * k, c.y - 32.0f * k, 64.0f * k, 64.0f * k);
    g.setColour (juce::Colour (0xff33353c));
    g.drawEllipse (c.x - 32.0f * k, c.y - 32.0f * k, 64.0f * k, 64.0f * k, juce::jmax (1.0f, 1.5f * k));
    g.setColour (juce::Colour (0xff212328));
    g.fillEllipse (c.x - 25.0f * k, c.y - 25.0f * k, 50.0f * k, 50.0f * k);

    juce::Path needle;
    needle.startNewSubPath (c.getPointOnCircumference (9.0f * k, angle));
    needle.lineTo (c.getPointOnCircumference (26.0f * k, angle));
    g.setColour (col::strong.withMultipliedAlpha (alpha));
    g.strokePath (needle, juce::PathStrokeType (juce::jmax (2.0f, 4.0f * k), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void JmcLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat();
    const bool selected = bg == col::accent;
    auto c = bg;
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.55f);
    else if (down) c = c.brighter (0.15f);
    else if (over && ! selected) c = juce::Colour (0xff17181c);
    g.setColour (c);
    g.fillRect (r);
    g.setColour (selected ? col::accent : (over ? col::mid : col::border));
    g.drawRect (r, 1.0f);
}

void JmcLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    const bool selected = b.findColour (juce::TextButton::buttonColourId) == col::accent;
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (! b.isEnabled() ? col::faint : (selected ? col::onAccent : (over ? col::strong : col::mid)));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, true);
}

// Switch like JMC VERB's ON toggle: 30 x 16 track, square knob, accent when on.
void JmcLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();
    const juce::Rectangle<float> trackR (r.getX() + 1.0f, r.getCentreY() - 8.0f, 30.0f, 16.0f);
    g.setColour (on ? col::accent : col::border);
    g.fillRoundedRectangle (trackR, 2.0f);
    g.setColour (on ? col::onAccent : col::dim);
    g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 12.0f).withPosition ({ trackR.getX() + (on ? 16.0f : 2.0f), trackR.getY() + 2.0f }), 1.0f);
    g.setColour (! b.isEnabled() ? col::faint : (on ? col::strong : col::dim));
    g.setFont (sans (10.0f, "Bold", 0.14f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (40.0f).toNearestInt(), juce::Justification::centredLeft, true);
}

void JmcLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isEnabled() ? col::panel : col::panel.withAlpha (0.5f));
    g.fillRect (r);
    g.setColour (col::border);
    g.drawRect (r, 1.0f);
    juce::Path chev;
    const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
    chev.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chev.lineTo (cx, cy + 2.0f);
    chev.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (col::dim);
    g.strokePath (chev, juce::PathStrokeType (1.3f));
}

void JmcLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::panel);
    g.setColour (col::border);
    g.drawRect (0, 0, w, h, 1);
}
} // namespace jmc::ui

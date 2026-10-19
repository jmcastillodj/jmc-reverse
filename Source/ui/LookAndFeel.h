#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// JMC family identity (shared with JMC VERB): matte graphite, hairline borders, vermilion accent
// for level/amount, ice blue for frequency/time, Helvetica Neue labels + Menlo values.
namespace jmc::ui
{
namespace col
{
    inline const juce::Colour bg       { 0xff121316 };
    inline const juce::Colour panel    { 0xff0d0e10 };   // wells / inputs
    inline const juce::Colour footer   { 0xff0f1012 };
    inline const juce::Colour line     { 0xff26282d };
    inline const juce::Colour lineSoft { 0xff1c1e22 };
    inline const juce::Colour border   { 0xff2c2e35 };
    inline const juce::Colour track    { 0xff25272c };
    inline const juce::Colour text     { 0xffe6e8ec };
    inline const juce::Colour strong   { 0xfff0f1f4 };
    inline const juce::Colour mid      { 0xffaeb3bd };
    inline const juce::Colour dim      { 0xff8a8f9a };
    inline const juce::Colour faint    { 0xff5b5f69 };
    inline const juce::Colour accent   { 0xffff4a2b };   // level / amount
    inline const juce::Colour ice      { 0xff7fa6d0 };   // frequency / time
    inline const juce::Colour ok       { 0xff4cc38a };
    inline const juce::Colour onAccent { 0xff121316 };   // text on the accent
    // aliases used across the editor
    inline const juce::Colour red      = accent;
    inline const juce::Colour redHi    { 0xffff6e52 };
    inline const juce::Colour panel2   { 0xff1a1b1f };   // control bodies
    inline const juce::Colour header   = bg;
}

inline juce::String sansName()
{
   #if JUCE_MAC
    return "Helvetica Neue";
   #else
    return juce::Font::getDefaultSansSerifFontName();
   #endif
}
inline juce::String monoName()
{
   #if JUCE_MAC
    return "Menlo";
   #else
    return juce::Font::getDefaultMonospacedFontName();
   #endif
}
inline juce::Font sans (float h, const juce::String& style = "Regular", float kerning = 0.0f)
{
    juce::Font f (juce::FontOptions (h).withName (sansName()).withStyle (style));
    f.setExtraKerningFactor (kerning);
    return f;
}
inline juce::Font mono (float h, bool bold = false, float kerning = 0.0f)
{
    juce::Font f (juce::FontOptions (h).withName (monoName()).withStyle (bold ? "Bold" : "Regular"));
    f.setExtraKerningFactor (kerning);
    return f;
}
// Tracked uppercase caption (JMC VERB: Helvetica Neue Bold 10, tracking 0.14)
inline juce::Font caps (float size, bool bold = false) { return sans (size, bold ? "Bold" : "Medium", 0.14f); }

class JmcLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JmcLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (11.0f, false, 0.06f); }
    juce::Font getPopupMenuFont() override { return sans (13.0f); }
    juce::Font getLabelFont (juce::Label&) override { return sans (12.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return sans (10.0f, "Bold", 0.14f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};
} // namespace jmc::ui

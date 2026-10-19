#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace jmc
{
// "Classic Reverse" -> "ClassicReverse"; anything not alphanumeric is dropped.
inline juce::String effectTag (const juce::String& name)
{
    juce::String out;
    bool up = true;
    for (auto c : name)
    {
        if (juce::CharacterFunctions::isLetterOrDigit (c))
        {
            out << (up ? juce::String::charToString (juce::CharacterFunctions::toUpperCase (c))
                       : juce::String::charToString (juce::CharacterFunctions::toLowerCase (c)));
            up = false;
        }
        else
            up = true;
    }
    return out.isEmpty() ? juce::String ("Custom") : out;
}

inline juce::String trimNumber (double v, int maxDecimals = 2)
{
    auto s = juce::String (v, maxDecimals);
    if (s.contains ("."))
        s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return s;
}

// JMCReverse_<Effect>_<N>bars_<bpm>bpm.wav, e.g. JMCReverse_ClassicReverse_4bars_120bpm.wav.
// N is the swell length in bars of the host time signature.
inline juce::String makeExportName (const juce::String& effect, double riserSeconds, double bpm, int tsNum, int tsDen)
{
    const double beats = riserSeconds * bpm / 60.0;
    const double perBar = (double) juce::jmax (1, tsNum) * 4.0 / (double) juce::jmax (1, tsDen);
    return "JMCReverse_" + effectTag (effect) + "_" + trimNumber (beats / perBar) + "bars_" + trimNumber (bpm) + "bpm.wav";
}
} // namespace jmc

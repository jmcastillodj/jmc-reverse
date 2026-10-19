// Prints the parameter table (Markdown) from the live parameter tree: jmc_params > docs/PARAMETERS.md
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>
#include <map>
#include <string>
#include "PluginProcessor.h"

static const std::map<std::string, const char*> kDesc = {
    { "mode", "Phrase (default): classic reverse reverb of the whole source, so the swell carries the spectrum of the whole phrase. Smooth tail: one continuous swell built only from the reverb tail that rings after the source (smoothest envelope, but its colour comes from the end of the phrase)." },
    { "length", "Length of the swell in seconds (free mode). With Tempo Sync on, the LENGTH knob drives Division instead." },
    { "decay", "Preferred reverb decay (RT60). Automatically limited to 0.25x-3x the length so the stretch stays natural." },
    { "size", "Room size: scales the reverb delay lines." },
    { "predelay", "Gap between the end of the swell and the source onset." },
    { "damping", "High-frequency damping inside the reverb loop." },
    { "lowcut", "High-pass filter applied to the source before the reverb." },
    { "highcut", "Low-pass filter applied to the source before the reverb." },
    { "width", "Stereo width (mid/side). Widening beyond 1 is limited so the mono sum never loses more than 6 dB." },
    { "dry", "Level of the original source appended after the swell. 0 = swell only." },
    { "sync", "Lock the length to the host tempo." },
    { "division", "Swell length in beats (4 beats = 1 bar in 4/4) when Tempo Sync is on." },
    { "preroll", "Silence before the swell starts." },
    { "tail", "Forward reverb tail of the source, starting at the onset." },
    { "lowdamp", "Extra low-frequency decay inside the reverb." },
    { "tilt", "Tonal tilt of the swell: negative darker, positive brighter." },
    { "balance", "Stereo balance of the swell." },
    { "curve", "Shape of the fade-in/fade-out." },
    { "pitch", "Pitch movement: the swell starts this many semitones away and arrives at the original pitch at the downbeat (negative = rise, positive = fall)." },
    { "chorus", "Enable the stereo chorus on the swell." },
    { "moddepth", "Chorus mix." },
    { "modrate", "Chorus rate." },
    { "rampon", "Enable the tempo-ramp tremolo: its speed travels through up to three note values over the length of the swell." },
    { "rampmix", "Ramp tremolo depth." },
    { "rampa", "Ramp tremolo speed at the start of the swell (note value at the host tempo)." },
    { "rampb", "Ramp tremolo speed in the middle of the swell; Auto = halfway between start and end." },
    { "rampc", "Ramp tremolo speed at the end of the swell." },
    { "tremolo", "Enable the tempo-synced tremolo (constant speed)." },
    { "tremmix", "Tremolo depth." },
    { "tremrate", "Tremolo timing (note value at the host tempo)." },
    { "gaterate", "Rhythmic stutter rate (note value at the host tempo); Off disables it." },
    { "gatedepth", "Stutter depth." },
    { "gateramp", "Makes the stutter accelerate towards the downbeat." },
    { "drive", "Soft saturation." },
    { "sibon", "Sibilance detection on/off: the swell is built mainly from detected S/SH regions." },
    { "sibsens", "Sibilance detection sensitivity." },
    { "sibamt", "How strongly detected sibilants drive the swell." },
    { "fadein", "Fade-in (attack) of the swell." },
    { "fadeout", "Fade-out (release) at the end of the swell." },
    { "outgain", "Final gain. Peaks above -0.1 dBFS are scaled, never clipped or limited." },
    { "trimstart", "Start of the source region used (fraction of the file). Set by dragging on the waveform." },
    { "trimend", "End of the source region used (fraction of the file). Set by dragging on the waveform." },
    { "autolevel", "Match the swell peak to the source peak (it only scales the swell)." },
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    JMCReverseProcessor p;
    std::printf ("# JMC REVERSE - parameters\n\n");
    std::printf ("Generated from the plugin by `jmc_params`; every row is a host-automatable VST3 parameter with a stable ID.\n");
    std::printf ("IDs are a contract with saved projects and are guarded by a unit test: never rename them.\n\n");
    std::printf ("| ID | Name | Type | Range | Default | Unit | What it does |\n|---|---|---|---|---|---|---|\n");
    int n = 0;
    for (auto* par : p.getParameters())
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (par);
        if (r == nullptr) continue;
        ++n;
        juce::String type = "float", range, def;
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (r))
        {
            type = "choice";
            range = c->choices.joinIntoString (" / ");
            def = c->choices[c->getIndex()];
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (r) != nullptr)
        {
            type = "toggle";
            range = "off / on";
            def = r->convertFrom0to1 (r->getDefaultValue()) > 0.5f ? "on" : "off";
        }
        else
        {
            range = r->getText (0.0f, 12) + " ... " + r->getText (1.0f, 12);
            def = r->getText (r->getDefaultValue(), 12);
        }
        const auto it = kDesc.find (r->getParameterID().toStdString());
        std::printf ("| `%s` | %s | %s | %s | %s | %s | %s |\n", r->getParameterID().toRawUTF8(), r->getName (64).toRawUTF8(), type.toRawUTF8(),
                     range.toRawUTF8(), def.toRawUTF8(), r->getLabel().toRawUTF8(), it != kDesc.end() ? it->second : "");
    }
    std::printf ("\n%d parameters.\n", n);
    return 0;
}

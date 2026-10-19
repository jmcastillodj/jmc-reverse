#pragma once
#include "Characters.h"
#include <string>

// Original factory presets, compiled in (no external files). Each starts from a character
// mode (0 = plain baseline) and overrides parameters. `sync` is set explicitly where a preset
// is meant to lock to the host tempo.
namespace jmc::factory
{
struct Preset { const char* category; const char* name; int base; characters::Values overrides; };

inline const std::vector<Preset>& all()
{
    namespace I = ids;
    static const std::vector<Preset> p = {
        // ---- Vocals
        { "Vocals", "Clean Vocal Rise",      2,  { { I::sync, 1 }, { I::division, 3 }, { I::decay, 1.8f } } },
        { "Vocals", "Intimate Whisper Swell", 3, { { I::length, 2.0f }, { I::decay, 1.2f }, { I::size, 0.8f }, { I::lowcut, 300.0f }, { I::damping, 0.2f }, { I::width, 1.1f } } },
        { "Vocals", "Pop Lead-In",           1,  { { I::sync, 1 }, { I::division, 3 }, { I::sibon, 1 }, { I::sibamt, 0.4f }, { I::tail, 0.8f } } },
        { "Vocals", "Warm Verse Pickup",     4,  { { I::sync, 1 }, { I::division, 3 }, { I::highcut, 6000.0f }, { I::tilt, -1.0f }, { I::decay, 2.0f }, { I::drive, 0.0f } } },
        { "Vocals", "Big Chorus Entry",      5,  { { I::sync, 1 }, { I::division, 4 }, { I::decay, 5.0f }, { I::width, 1.6f }, { I::tail, 1.5f }, { I::predelay, 10.0f } } },
        { "Vocals", "Sibilant Sparkle",      12, { { I::sync, 1 }, { I::division, 3 }, { I::sibamt, 0.9f }, { I::tilt, 5.0f } } },
        // ---- Ad-libs
        { "Ad-libs", "Ad-lib Throw",         1,  { { I::sync, 1 }, { I::division, 2 }, { I::decay, 1.2f }, { I::width, 1.4f }, { I::sibon, 0 } } },
        { "Ad-libs", "Wide Ad-lib Swell",    10, { { I::sync, 1 }, { I::division, 3 }, { I::width, 1.8f }, { I::decay, 2.2f } } },
        { "Ad-libs", "Pitched Echo Pickup",  6,  { { I::sync, 1 }, { I::division, 2 }, { I::pitch, -3.0f }, { I::decay, 1.0f }, { I::size, 0.7f } } },
        { "Ad-libs", "Ad-lib Glitch",        9,  { { I::sync, 1 }, { I::division, 2 }, { I::gaterate, 3 }, { I::decay, 1.0f } } },
        { "Ad-libs", "Whisper Ad-lib",       3,  { { I::sync, 1 }, { I::division, 2 }, { I::decay, 1.0f }, { I::size, 0.8f }, { I::lowcut, 400.0f } } },
        // ---- Transitions
        { "Transitions", "Classic 4-Beat Riser", 1, { { I::sync, 1 }, { I::division, 4 } } },
        { "Transitions", "8-Beat Build",     1,  { { I::sync, 1 }, { I::division, 5 }, { I::decay, 4.0f }, { I::tilt, 1.0f }, { I::fadein, 0.4f } } },
        { "Transitions", "16-Beat Epic Build", 5, { { I::sync, 1 }, { I::division, 6 }, { I::decay, 8.0f }, { I::chorus, 1 }, { I::moddepth, 0.25f }, { I::tail, 2.0f } } },
        { "Transitions", "Downbeat Impact",  11, { { I::sync, 1 }, { I::division, 4 } } },
        { "Transitions", "Last-Beat Snap",   6,  { { I::sync, 1 }, { I::division, 2 }, { I::decay, 0.5f }, { I::curve, 2 } } },
        { "Transitions", "Pitch Lift 2 Bars", 7, { { I::sync, 1 }, { I::division, 5 }, { I::pitch, -12.0f }, { I::decay, 4.0f } } },
        { "Transitions", "Falling Tape Stop", 8, { { I::sync, 1 }, { I::division, 3 }, { I::pitch, 12.0f }, { I::curve, 3 } } },
        // ---- Sound Design
        { "Sound Design", "Dark Cavern",     5,  { { I::decay, 12.0f }, { I::highcut, 2500.0f }, { I::tilt, -5.0f }, { I::lowdamp, 0.0f }, { I::length, 12.0f } } },
        { "Sound Design", "Glass Air",       3,  { { I::lowcut, 1200.0f }, { I::tilt, 6.0f }, { I::chorus, 1 }, { I::moddepth, 0.5f }, { I::modrate, 0.7f }, { I::width, 1.8f } } },
        { "Sound Design", "Rhythmic Pulse",  9,  { { I::sync, 1 }, { I::division, 4 }, { I::gaterate, 2 }, { I::gateramp, 0.0f }, { I::gatedepth, 0.85f } } },
        { "Sound Design", "Warped Wide Pad", 10, { { I::length, 8.0f }, { I::chorus, 1 }, { I::moddepth, 0.8f }, { I::modrate, 0.3f }, { I::width, 2.0f }, { I::pitch, -5.0f }, { I::decay, 6.0f } } },
        { "Sound Design", "Lo-fi Dust",       4,  { { I::drive, 0.6f }, { I::highcut, 4000.0f }, { I::lowcut, 120.0f }, { I::chorus, 1 }, { I::moddepth, 0.2f }, { I::modrate, 0.4f } } },
        { "Sound Design", "Ramp Trem Build", 1,  { { I::sync, 1 }, { I::division, 4 }, { I::rampon, 1 }, { I::rampmix, 0.9f }, { I::rampa, 2 }, { I::rampb, 0 }, { I::rampc, 4 }, { I::fadein, 0.3f } } },
        { "Sound Design", "Ramp Trem Fall",  1,  { { I::sync, 1 }, { I::division, 4 }, { I::rampon, 1 }, { I::rampmix, 0.9f }, { I::rampa, 4 }, { I::rampb, 0 }, { I::rampc, 1 }, { I::fadein, 0.2f } } },
        { "Sound Design", "Tremolo Build",   1,  { { I::sync, 1 }, { I::division, 4 }, { I::tremolo, 1 }, { I::tremmix, 0.8f }, { I::tremrate, 3 }, { I::fadein, 0.3f } } },
        { "Sound Design", "Sub Drop Swell",  4,  { { I::length, 5.0f }, { I::decay, 6.0f }, { I::tilt, -6.0f }, { I::highcut, 1800.0f }, { I::pitch, 12.0f }, { I::curve, 2 } } },
    };
    return p;
}

inline characters::Values resolve (const Preset& p)
{
    auto v = characters::baseline();
    v.push_back ({ ids::sync, 0.0f });
    auto set = [&v] (const char* id, float value)
    {
        for (auto& e : v) if (std::string (e.first) == id) { e.second = value; return; }
        v.push_back ({ id, value });
    };
    if (p.base >= 1 && p.base <= characters::count)
        for (const auto& o : characters::overrides (p.base - 1)) set (o.first, o.second);
    for (const auto& o : p.overrides) set (o.first, o.second);
    return v;
}
} // namespace jmc::factory

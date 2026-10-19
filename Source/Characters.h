#pragma once
#include "ParameterIDs.h"
#include <utility>
#include <vector>

// One-click character modes. Each is a full parameter configuration: baseline values plus
// overrides, applied through the normal parameters, so everything stays editable afterwards.
// Sync/trim/output-gain/Auto-Level-independent settings (sync, trim) are never touched.
namespace jmc::characters
{
using Values = std::vector<std::pair<const char*, float>>;

inline constexpr int count = 12;
inline constexpr const char* names[count] = {
    "CLASSIC REVERSE", "VOCAL LIFT", "AIR SWELL", "DARK REVERSE", "HUGE SPACE", "TIGHT REVERSE",
    "PITCH RISE", "PITCH FALL", "GLITCH REVERSE", "WIDE REVERSE", "IMPACT BUILD", "SIBILANCE LIFT" };

inline Values baseline()
{
    namespace I = ids;
    return {
        { I::mode, 1.0f }, { I::length, 4.0f }, { I::division, 4.0f }, { I::decay, 2.5f }, { I::size, 1.0f }, { I::predelay, 0.0f },
        { I::damping, 0.4f }, { I::lowdamp, 0.2f }, { I::lowcut, 20.0f }, { I::highcut, 20000.0f }, { I::tilt, 0.0f },
        { I::width, 1.0f }, { I::balance, 0.0f }, { I::dry, 0.0f }, { I::preroll, 0.0f }, { I::tail, 0.0f },
        { I::fadein, 0.05f }, { I::fadeout, 0.005f }, { I::curve, 0.0f }, { I::pitch, 0.0f },
        { I::chorus, 0.0f }, { I::moddepth, 0.5f }, { I::modrate, 0.8f }, { I::rampon, 0.0f }, { I::rampmix, 1.0f },
        { I::rampa, 2.0f }, { I::rampb, 0.0f }, { I::rampc, 4.0f },
        { I::tremolo, 0.0f }, { I::tremmix, 1.0f }, { I::tremrate, 2.0f }, { I::gaterate, 0.0f }, { I::gatedepth, 0.8f }, { I::gateramp, 0.0f },
        { I::drive, 0.0f }, { I::sibon, 0.0f }, { I::sibsens, 0.5f }, { I::sibamt, 0.7f }, { I::outgain, 0.0f },
        { I::autolevel, 1.0f } };
}

// Overrides on top of baseline(). Choice values are indices.
inline Values overrides (int idx)
{
    namespace I = ids;
    switch (idx)
    {
        case 0:  return {}; // CLASSIC REVERSE: the baseline itself
        case 1:  return { { I::length, 2.0f }, { I::division, 3.0f }, { I::decay, 2.0f }, { I::damping, 0.25f }, { I::lowcut, 150.0f },
                          { I::tilt, 3.0f }, { I::width, 1.2f }, { I::fadein, 0.3f }, { I::curve, 2.0f }, { I::sibon, 1.0f }, { I::sibamt, 0.5f } };
        case 2:  return { { I::length, 6.0f }, { I::division, 5.0f }, { I::decay, 4.0f }, { I::size, 1.6f }, { I::damping, 0.1f },
                          { I::lowcut, 600.0f }, { I::tilt, 5.0f }, { I::width, 1.5f }, { I::chorus, 1.0f }, { I::moddepth, 0.3f }, { I::modrate, 0.4f }, { I::fadein, 0.4f } };
        case 3:  return { { I::decay, 3.0f }, { I::damping, 0.75f }, { I::lowdamp, 0.0f }, { I::lowcut, 60.0f }, { I::highcut, 3500.0f },
                          { I::tilt, -4.0f }, { I::drive, 0.2f } };
        case 4:  return { { I::length, 10.0f }, { I::division, 6.0f }, { I::decay, 9.0f }, { I::size, 1.8f }, { I::predelay, 20.0f },
                          { I::damping, 0.3f }, { I::width, 1.5f }, { I::tail, 2.0f }, { I::fadein, 0.5f } };
        case 5:  return { { I::length, 0.75f }, { I::division, 1.0f }, { I::decay, 0.6f }, { I::size, 0.6f }, { I::damping, 0.5f },
                          { I::fadein, 0.02f }, { I::curve, 2.0f } };
        case 6:  return { { I::pitch, -7.0f }, { I::decay, 2.5f }, { I::damping, 0.35f }, { I::tilt, 1.0f } };
        case 7:  return { { I::length, 3.0f }, { I::division, 3.0f }, { I::pitch, 7.0f }, { I::decay, 2.0f }, { I::damping, 0.45f } };
        case 8:  return { { I::length, 2.0f }, { I::division, 3.0f }, { I::decay, 1.5f }, { I::damping, 0.4f }, { I::gaterate, 2.0f },
                          { I::gatedepth, 0.95f }, { I::gateramp, 0.6f }, { I::drive, 0.25f }, { I::fadein, 0.01f },
                          { I::sibon, 1.0f }, { I::sibamt, 0.4f } };
        case 9:  return { { I::decay, 3.5f }, { I::size, 1.4f }, { I::width, 2.0f }, { I::chorus, 1.0f }, { I::moddepth, 0.5f }, { I::modrate, 0.6f }, { I::tilt, 1.0f } };
        case 10: return { { I::decay, 3.0f }, { I::predelay, 40.0f }, { I::damping, 0.35f }, { I::lowcut, 100.0f }, { I::pitch, -3.0f },
                          { I::gaterate, 2.0f }, { I::gatedepth, 0.35f }, { I::gateramp, 0.5f }, { I::drive, 0.35f }, { I::dry, 1.0f },
                          { I::tail, 2.0f }, { I::fadein, 0.5f }, { I::fadeout, 0.001f }, { I::curve, 2.0f } };
        case 11: return { { I::length, 3.0f }, { I::division, 3.0f }, { I::decay, 2.0f }, { I::damping, 0.2f }, { I::lowcut, 200.0f },
                          { I::tilt, 4.0f }, { I::fadein, 0.1f }, { I::sibon, 1.0f }, { I::sibsens, 0.6f }, { I::sibamt, 1.0f } };
        default: return {};
    }
}

// Full resolved configuration for a character (index 0..count-1).
inline Values resolve (int idx)
{
    auto v = baseline();
    for (const auto& o : overrides (idx))
        for (auto& b : v)
            if (std::string (b.first) == o.first) { b.second = o.second; break; }
    return v;
}
} // namespace jmc::characters

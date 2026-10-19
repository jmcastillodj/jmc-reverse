#pragma once
#include "ParameterIDs.h"
#include "dsp/RenderSettings.h"
#include "dsp/Timing.h"
#include <cmath>

namespace jmc
{
// Maps parameter values (in real units; choices as indices) to renderer settings.
// Shared by the plugin and the tests. Sibilance regions are added by the caller.
template <typename Getter>
RenderSettings settingsFromParams (Getter g, double hostBpm)
{
    RenderSettings s;
    s.durationSec = g (ids::length);
    if (g (ids::sync) > 0.5f)
    {
        const int idx = std::clamp ((int) std::lround (g (ids::division)), 0, timing::numDivisions - 1);
        s.durationSec = timing::beatsToSeconds (timing::divisionBeats[idx], hostBpm);
    }
    s.mode        = (int) std::lround (g (ids::mode));
    s.lowDamp     = g (ids::lowdamp);
    s.tiltDb      = g (ids::tilt);
    s.balance     = g (ids::balance);
    s.curve       = (int) std::lround (g (ids::curve));
    s.pitchSemis  = g (ids::pitch);
    s.modDepth    = g (ids::chorus) > 0.5f ? g (ids::moddepth) : 0.0f;
    s.rampMix     = g (ids::rampon) > 0.5f ? g (ids::rampmix) : 0.0f;
    {
        static const float lg[7] = { -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f }; // 1/1 ... 1/64 = 0.25 ... 16 cycles per beat
        const int ia = std::clamp ((int) std::lround (g (ids::rampa)), 0, 6), ic = std::clamp ((int) std::lround (g (ids::rampc)), 0, 6);
        const int ib = std::clamp ((int) std::lround (g (ids::rampb)), 0, 7);   // 0 = Auto (halfway between start and end)
        s.rampA = lg[ia];
        s.rampC = lg[ic];
        s.rampB = ib == 0 ? 0.5f * (s.rampA + s.rampC) : lg[ib - 1];
    }
    s.tremMix     = g (ids::tremolo) > 0.5f ? g (ids::tremmix) : 0.0f;
    {
        static const float cyc[5] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f }; // 1/2, 1/4, 1/8, 1/16, 1/32 note
        s.tremRate = cyc[std::clamp ((int) std::lround (g (ids::tremrate)), 0, 4)];
    }
    s.modRate     = g (ids::modrate);
    s.gateRate    = (int) std::lround (g (ids::gaterate));
    s.gateDepth   = g (ids::gatedepth);
    s.gateRamp    = g (ids::gateramp);
    s.bpm         = (s.gateRate > 0 || s.tremMix > 0.0f || s.rampMix > 0.0f) ? (float) hostBpm : 0.0f;
    s.drive       = g (ids::drive);
    s.preRollSec  = g (ids::preroll);
    s.tailSec     = g (ids::tail);
    s.decaySec    = g (ids::decay);
    s.size        = g (ids::size);
    s.predelayMs  = g (ids::predelay);
    s.damping     = g (ids::damping);
    s.lowCutHz    = g (ids::lowcut);
    s.highCutHz   = g (ids::highcut);
    s.width       = g (ids::width);
    s.dryLevel    = g (ids::dry);
    s.fadeInSec   = g (ids::fadein);
    s.fadeOutSec  = g (ids::fadeout);
    s.outGainDb   = g (ids::outgain);
    s.trimStart   = g (ids::trimstart);
    s.trimEnd     = g (ids::trimend);
    s.autoLevel   = g (ids::autolevel) > 0.5f;
    return s;
}
} // namespace jmc

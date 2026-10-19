#pragma once
// Stable parameter identifiers. Never rename or renumber: hosts store these.
namespace jmc::ids
{
    inline constexpr const char* mode      = "mode";
    inline constexpr const char* length    = "length";
    inline constexpr const char* decay     = "decay";
    inline constexpr const char* size      = "size";
    inline constexpr const char* predelay  = "predelay";
    inline constexpr const char* damping   = "damping";
    inline constexpr const char* lowcut    = "lowcut";
    inline constexpr const char* highcut   = "highcut";
    inline constexpr const char* width     = "width";
    inline constexpr const char* dry       = "dry";
    inline constexpr const char* sync      = "sync";
    inline constexpr const char* division  = "division";
    inline constexpr const char* preroll   = "preroll";
    inline constexpr const char* tail      = "tail";
    inline constexpr const char* sibon     = "sibon";
    inline constexpr const char* sibsens   = "sibsens";
    inline constexpr const char* sibamt    = "sibamt";
    inline constexpr const char* lowdamp   = "lowdamp";
    inline constexpr const char* tilt      = "tilt";
    inline constexpr const char* balance   = "balance";
    inline constexpr const char* curve     = "curve";
    inline constexpr const char* pitch     = "pitch";
    inline constexpr const char* moddepth  = "moddepth";
    inline constexpr const char* modrate   = "modrate";
    inline constexpr const char* gaterate  = "gaterate";
    inline constexpr const char* gatedepth = "gatedepth";
    inline constexpr const char* gateramp  = "gateramp";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* chorus    = "chorus";
    inline constexpr const char* rampon    = "rampon";
    inline constexpr const char* rampmix   = "rampmix";
    inline constexpr const char* rampa     = "rampa";
    inline constexpr const char* rampb     = "rampb";
    inline constexpr const char* rampc     = "rampc";
    inline constexpr const char* tremolo   = "tremolo";
    inline constexpr const char* tremmix   = "tremmix";
    inline constexpr const char* tremrate  = "tremrate";
    inline constexpr const char* fadein    = "fadein";
    inline constexpr const char* fadeout   = "fadeout";
    inline constexpr const char* outgain   = "outgain";
    inline constexpr const char* autolevel = "autolevel";
    inline constexpr const char* trimstart = "trimstart";
    inline constexpr const char* trimend   = "trimend";
}

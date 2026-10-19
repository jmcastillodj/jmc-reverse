#pragma once
#include "Sibilance.h"
#include <functional>
#include <vector>
#include <cstddef>

#if defined(__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wfloat-equal" // exact comparison is intended: detects any change
#endif

namespace jmc
{
// Everything the offline renderer needs. Plain data so it can be snapshotted, compared and tested
// without the plugin framework.
struct RenderSettings
{
    int   mode        = 1;      // 1 = phrase (classic reverse reverb of the whole source, default), 0 = smooth tail (continuous riser from the reverb tail)
    float durationSec = 4.0f;   // length of the generated riser; <= 0 keeps the natural (unstretched) length
    float decaySec    = 2.5f;   // preferred reverb RT60 (auto-clamped relative to duration)
    float size        = 1.0f;   // scales FDN delay lengths (0.25 .. 2)
    float predelayMs  = 0.0f;   // gap between end of swell and source onset
    float damping     = 0.4f;   // 0..1 high-frequency damping inside the reverb loop
    float lowCutHz    = 20.0f;  // <= 25 disables
    float highCutHz   = 20000.0f; // >= 19000 disables
    float width       = 1.0f;   // 0 mono .. 2 wide (mid/side)
    float dryLevel    = 0.0f;   // > 0 appends the original source (forward) after the swell
    float preRollSec  = 0.0f;   // silence before the swell starts
    float tailSec     = 0.0f;   // forward reverb tail of the source, starting at the onset
    float fadeInSec   = 0.05f;
    float fadeOutSec  = 0.005f;
    float outGainDb   = 0.0f;
    float trimStart   = 0.0f;   // fractions of the source
    float trimEnd     = 1.0f;
    float lowDamp     = 0.2f;   // 0..1 extra low-frequency decay inside the reverb
    float tiltDb      = 0.0f;   // tonal tilt: +ve = brighter (high shelf up, low shelf down)
    float balance     = 0.0f;   // -1 left .. +1 right
    int   curve       = 0;      // fade shape: 0 S-curve, 1 linear, 2 exponential, 3 logarithmic
    float pitchSemis  = 0.0f;   // pitch offset at the start of the swell, gliding to 0 at the onset (-12..12)
    float modDepth    = 0.0f;   // stereo chorus-style modulation
    float modRate     = 0.8f;   // Hz
    int   gateRate    = 0;      // 0 off, 1 = 1/8, 2 = 1/16, 3 = 1/32 note stutter
    float gateDepth   = 0.8f;
    float gateRamp    = 0.0f;   // 0..1: stutter speeds up towards the onset
    float bpm         = 0.0f;   // only set when the gate is on (tempo-locked stutter)
    float rampMix     = 0.0f;   // 0 = off; tempo-ramped tremolo depth
    float rampA = 0.0f, rampB = 0.0f, rampC = 0.0f; // its start / mid / end speed as log2(cycles per beat)
    float tremMix     = 0.0f;   // 0 = off; tempo-synced tremolo depth
    float tremRate    = 2.0f;   // tremolo cycles per beat (needs bpm)
    float drive       = 0.0f;   // soft saturation
    float sibAmount   = 0.0f;   // 0 = off. Emphasises the regions below in the reverb input
    std::vector<SibRegion> sibRegions; // fractions of the full (untrimmed) source
    bool  autoLevel   = true;   // match riser peak to the source peak

    bool operator== (const RenderSettings&) const = default;
};

// Inputs that determine the expensive first stages (trim, pre-filter, sibilance emphasis, reverb,
// stretch, glide). Anything downstream (tone, FX, envelope, width, levels, dry, tail, export) can
// change without recomputing them.
struct RenderCacheKey
{
    const void* srcPtr = nullptr;
    size_t n = 0;
    double sr = 0;
    unsigned long long fingerprint = 0;
    float trimStart = 0, trimEnd = 0, duration = 0, decay = 0, size = 0, damping = 0, lowDamp = 0, predelay = 0,
          lowCut = 0, highCut = 0, sibAmount = 0, pitch = 0;
    bool tail = false;
    int mode = 0;
    int channels = 0;
    std::vector<SibRegion> regions;
    bool operator== (const RenderCacheKey&) const = default;
};

struct RenderCache
{
    bool valid = false;
    RenderCacheKey key;
    std::vector<float> riser[2], dry[2], fwd[2];
};

struct RenderResult
{
    std::vector<float> ch[2];   // always two channels
    size_t riserSamples = 0;    // length of the swell portion
    size_t onsetSamples = 0;    // preRoll + swell: where the source onset / downbeat lands in the file
    float correlation = 0.0f;   // L/R correlation of the final render
    float monoLossDb = 0.0f;    // level change of the mono sum vs the stereo average
    float effectiveWidth = 1.0f; // width after the mono-compatibility safeguard
    bool ok = false;
    bool cancelled = false;
    size_t length() const { return ch[0].size(); }
};

// in: numCh (1 or 2) channel pointers, n samples at sampleRate. Pure C++, offline, allocates.
// `cache` (optional, single-threaded use) lets repeated renders of the same source skip the reverb
// when only downstream settings changed.
RenderResult renderReverseReverb (const float* const* in, int numCh, size_t n, double sampleRate,
                                  const RenderSettings&, const std::function<bool()>& shouldCancel = {},
                                  RenderCache* cache = nullptr);
} // namespace jmc

#if defined(__clang__)
 #pragma clang diagnostic pop
#endif

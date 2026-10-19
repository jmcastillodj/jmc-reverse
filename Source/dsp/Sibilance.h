#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>

#if defined(__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wfloat-equal"
#endif
namespace jmc
{
// Region of the source as fractions (0..1) of its full length.
struct SibRegion
{
    float start = 0.0f, end = 0.0f;
    bool operator== (const SibRegion&) const = default;
};

// Per-frame spectral features. Independent of sensitivity, so changing sensitivity
// only re-runs the cheap threshold stage (detectSibilance).
struct SibFeatures
{
    double sampleRate = 0.0;
    size_t numSamples = 0;
    size_t hop = 0, frameLen = 0;
    std::vector<float> ratio;    // energy(4.5-11 kHz) / energy(80 Hz-Nyquist)
    std::vector<float> centroid; // Hz, over 80 Hz-Nyquist
    std::vector<float> flatness; // spectral flatness of the sibilant band (noise ~0.5, tonal ~0)
    std::vector<float> levelDb;  // sibilant-band level
    std::vector<float> hiLoDb;   // sibilant band vs 150 Hz-3 kHz
    float maxLevelDb = -200.0f;
};

struct SibResult
{
    std::vector<SibRegion> regions;
    bool reliable = false;       // false -> regions is empty
    float coverage = 0.0f;       // fraction of the source covered by regions
    std::string message;
};

// Spectral/temporal heuristics (no machine learning). Stereo: each channel is analysed and
// the frame takes the stronger channel, so hard-panned and out-of-phase material still works.
std::shared_ptr<SibFeatures> analyseSibilance (const float* const* ch, int numCh, size_t n, double sampleRate,
                                               const std::function<bool()>& cancel = {});
SibResult detectSibilance (const SibFeatures&, float sensitivity /*0..1*/);
} // namespace jmc
#if defined(__clang__)
 #pragma clang diagnostic pop
#endif

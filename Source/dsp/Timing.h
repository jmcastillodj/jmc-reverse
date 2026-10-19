#pragma once
#include <algorithm>
#include <cmath>

// Musical timing helpers (pure functions, unit-tested).
namespace jmc::timing
{
inline constexpr int numDivisions = 7;
inline constexpr float divisionBeats[numDivisions] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
inline constexpr const char* divisionNames[numDivisions] = { "1/4 beat", "1/2 beat", "1 beat", "2 beats", "4 beats", "8 beats", "16 beats" };
inline constexpr double minDuration = 0.1, maxDuration = 30.0;

inline double sanitiseBpm (double bpm) { return (std::isfinite (bpm) && bpm >= 20.0 && bpm <= 400.0) ? std::round (bpm * 100.0) / 100.0 : 120.0; }

inline float beatsToSeconds (double beats, double bpm)
{
    return (float) std::clamp (beats * 60.0 / sanitiseBpm (bpm), minDuration, maxDuration);
}

// Bars from beats given a time signature (numerator/denominator): a bar holds num * 4/den quarter-note beats.
inline double beatsToBars (double beats, int num, int den)
{
    const double perBar = (double) std::max (1, num) * 4.0 / (double) std::max (1, den);
    return beats / perBar;
}
} // namespace jmc::timing

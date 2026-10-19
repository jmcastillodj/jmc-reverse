#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Offline creative/tonal processors used by the renderer. Pure functions on two-channel buffers.
namespace jmc::fx
{
constexpr double kPi = 3.14159265358979323846;

// 0 S-curve, 1 linear, 2 exponential, 3 logarithmic; x and result in 0..1.
inline double fadeShape (int curve, double x)
{
    x = std::clamp (x, 0.0, 1.0);
    switch (curve)
    {
        case 1:  return x;
        case 2:  return x * x * x;
        case 3:  return 1.0 - (1.0 - x) * (1.0 - x);
        default: return 0.5 - 0.5 * std::cos (kPi * x);
    }
}

inline void tilt (std::vector<float>& l, std::vector<float>& r, float tiltDb, double sr)
{
    if (std::fabs (tiltDb) < 0.05f) return;
    for (auto* ch : { &l, &r })
    {
        auto lo = Biquad::makeShelf (false, 700.0, -0.5 * (double) tiltDb, sr);
        auto hi = Biquad::makeShelf (true,  700.0,  0.5 * (double) tiltDb, sr);
        lo.process (*ch);
        hi.process (*ch);
    }
}

// Stereo chorus-style modulation: short modulated delay, quadrature LFOs for L/R.
inline void modulate (std::vector<float>& l, std::vector<float>& r, float depth, float rateHz, double sr)
{
    if (depth < 0.01f) return;
    const size_t n = l.size();
    const double base = 0.008 * sr, swing = 0.006 * sr * (double) depth;
    const std::vector<float> src[2] = { l, r };
    std::vector<float>* dst[2] = { &l, &r };
    for (int c = 0; c < 2; ++c)
        for (size_t i = 0; i < n; ++i)
        {
            const double d = base + swing * std::sin (2 * kPi * (double) rateHz * (double) i / sr + c * 0.5 * kPi);
            const double p = (double) i - d;
            float w = 0.0f;
            if (p >= 0)
            {
                const size_t i0 = (size_t) p;
                const float fr = (float) (p - (double) i0);
                const float a = src[c][i0], b = i0 + 1 < n ? src[c][i0 + 1] : a;
                w = a + (b - a) * fr;
            }
            (*dst[c])[i] = src[c][i] * (1.0f - 0.5f * depth) + w * 0.5f * depth;
        }
}

// Tempo-locked stutter gate; with ramp the rate accelerates up to 4x by the end.
inline void stutter (std::vector<float>& l, std::vector<float>& r, int rateIdx, float depth, float ramp, double bpm, double sr)
{
    if (rateIdx <= 0 || depth < 0.01f || bpm <= 0) return;
    const double cyclesPerBeat = rateIdx == 1 ? 2.0 : rateIdx == 2 ? 4.0 : 8.0;
    const double f0 = cyclesPerBeat * bpm / 60.0;
    const size_t n = l.size();
    double phase = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double x = (double) i / (double) std::max<size_t> (1, n - 1);
        const double f = f0 * (1.0 + 3.0 * (double) ramp * x * x);
        const double e = std::min (0.25, 0.002 * f);
        const double u = phase - std::floor (phase);
        const double open = u < 0.5 ? std::clamp (u / e, 0.0, 1.0) * std::clamp ((0.5 - u) / e, 0.0, 1.0) : 0.0;
        const float g = (float) (1.0 - (double) depth * (1.0 - open));
        l[i] *= g;
        r[i] *= g;
        phase += f / sr;
    }
}

// Tempo-ramped tremolo: the tremolo speed travels through three note values (start -> mid -> end)
// over the length of the swell, gliding smoothly between them (cosine easing in the log domain, so
// 1/4 -> 1/16 passes through 1/8). a, b, c are log2(cycles per beat). Locked to the host tempo.
inline void rampTremolo (std::vector<float>& l, std::vector<float>& r, float mix, float a, float b, float c, double bpm, double sr)
{
    if (mix < 0.01f || bpm <= 0 || l.empty()) return;
    const size_t n = l.size();
    double phase = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double x = (double) i / (double) std::max<size_t> (1, n - 1);
        const double s = x < 0.5 ? x * 2.0 : (x - 0.5) * 2.0;
        const double w = 0.5 - 0.5 * std::cos (kPi * s);
        const double lg = x < 0.5 ? (double) a + ((double) b - (double) a) * w : (double) b + ((double) c - (double) b) * w;
        const double f = std::pow (2.0, lg) * bpm / 60.0; // Hz
        const float g = 1.0f - mix * (float) (0.5 - 0.5 * std::cos (2 * kPi * phase));
        l[i] *= g;
        r[i] *= g;
        phase += f / sr;
        phase -= std::floor (phase);
    }
}

// Tempo-synced amplitude tremolo: gain swings between 1 and (1 - mix) once per cycle.
inline void tremolo (std::vector<float>& l, std::vector<float>& r, float mix, double cyclesPerBeat, double bpm, double sr)
{
    if (mix < 0.01f || bpm <= 0 || cyclesPerBeat <= 0) return;
    const double f = cyclesPerBeat * bpm / 60.0;
    for (size_t i = 0; i < l.size(); ++i)
    {
        const double ph = f * (double) i / sr;
        const float g = 1.0f - mix * (float) (0.5 - 0.5 * std::cos (2 * kPi * ph));
        l[i] *= g;
        r[i] *= g;
    }
}

inline void saturate (std::vector<float>& l, std::vector<float>& r, float drive)
{
    if (drive < 0.01f) return;
    float pk = 0;
    for (float v : l) pk = std::max (pk, std::fabs (v));
    for (float v : r) pk = std::max (pk, std::fabs (v));
    if (pk < 1.0e-12f) return;
    const float k = 1.0f + 9.0f * drive, norm = 1.0f / std::tanh (k);
    for (auto* ch : { &l, &r })
        for (auto& v : *ch) v = std::tanh (k * v / pk) * norm * pk;
}

// Per-sample read rates for a pitch glide that starts `semis` away from the original and
// arrives at the original pitch at the end. Returns the total source length consumed.
inline double glideRates (std::vector<double>& rates, size_t len, double semis)
{
    rates.resize (len);
    double sum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        const double x = (double) i / (double) std::max<size_t> (1, len - 1);
        rates[i] = std::pow (2.0, semis * (1.0 - x) / 12.0);
        sum += rates[i];
    }
    return sum;
}

// Variable-rate Catmull-Rom read: out[i] = in(pos), pos += rates[i] * scale.
inline std::vector<float> glideResample (const std::vector<float>& in, const std::vector<double>& rates, double scale)
{
    std::vector<float> out (rates.size());
    const long n = (long) in.size();
    double pos = 0;
    auto at = [&] (long i) { return in[(size_t) std::clamp<long> (i, 0, n - 1)]; };
    for (size_t i = 0; i < rates.size(); ++i)
    {
        const long i1 = (long) pos;
        const float t = (float) (pos - (double) i1);
        const float p0 = at (i1 - 1), p1 = at (i1), p2 = at (i1 + 1), p3 = at (i1 + 2);
        out[i] = p1 + 0.5f * t * (p2 - p0 + t * (2 * p0 - 5 * p1 + 4 * p2 - p3 + t * (3 * (p1 - p2) + p3 - p0)));
        pos += rates[i] * scale;
    }
    return out;
}

// Mid/side width with a mono-compatibility safeguard: widening beyond 1 is limited so the
// mono sum never loses more than ~6 dB (L/R correlation stays >= -0.5). Also applies balance.
inline double widen (std::vector<float>& l, std::vector<float>& r, float width, float balance)
{
    double em = 0, es = 0;
    for (size_t i = 0; i < l.size(); ++i)
    {
        const double m = 0.5 * ((double) l[i] + r[i]), sd = 0.5 * ((double) l[i] - r[i]);
        em += m * m; es += sd * sd;
    }
    double w = std::clamp ((double) width, 0.0, 2.0);
    if (w > 1.0 && es > 1.0e-20)
        w = std::clamp (std::sqrt (3.0 * em / es), 1.0, w) ;   // never narrower than the natural image
    const float gl = std::min (1.0f, 1.0f - balance), gr = std::min (1.0f, 1.0f + balance);
    for (size_t i = 0; i < l.size(); ++i)
    {
        const float m = 0.5f * (l[i] + r[i]), sd = 0.5f * (l[i] - r[i]) * (float) w;
        l[i] = (m + sd) * gl;
        r[i] = (m - sd) * gr;
    }
    return w;
}

inline void monoStats (const std::vector<float>& l, const std::vector<float>& r, float& corr, float& lossDb)
{
    double ll = 0, rr = 0, lr = 0, mm = 0;
    for (size_t i = 0; i < l.size(); ++i)
    {
        ll += (double) l[i] * l[i]; rr += (double) r[i] * r[i]; lr += (double) l[i] * r[i];
        const double m = 0.5 * ((double) l[i] + r[i]);
        mm += m * m;
    }
    corr = (ll > 1e-20 && rr > 1e-20) ? (float) (lr / std::sqrt (ll * rr)) : 1.0f;
    lossDb = (ll + rr > 1e-20) ? (float) (10.0 * std::log10 (std::max (1e-12, mm / (0.5 * (ll + rr))))) : 0.0f;
}
} // namespace jmc::fx

#include "FdnReverb.h"
#include <algorithm>
#include <cmath>

namespace jmc
{
static const double baseMs[FdnReverb::N] = { 31.7, 37.3, 43.9, 51.1, 59.3, 67.7, 77.9, 89.3 };

void FdnReverb::prepare (double sr, float decaySec, float size, float damping, float predelaySec, float lowDamp)
{
    constexpr double pi = 3.14159265358979323846;
    decaySec = std::max (decaySec, 0.05f);
    size = std::clamp (size, 0.25f, 2.0f);
    damp = 0.85f * std::clamp (damping, 0.0f, 1.0f);
    modDepth = 0.0025 * sr; // +-2.5 ms at slow rates smears the modes: no static ringing, <15 cents of pitch drift
    lowK = 0.6f * std::clamp (lowDamp, 0.0f, 1.0f);   // extra low-frequency loss per pass (below ~250 Hz)
    lfCoef = (float) std::exp (-2.0 * pi * 250.0 / sr);

    for (int i = 0; i < N; ++i)
    {
        auto& l = lines[(size_t) i];
        l.len = std::max (16.0, std::round (baseMs[i] * size * 0.001 * sr));
        size_t cap = 1;
        while ((double) cap < l.len + modDepth + 8) cap <<= 1;
        l.buf.assign (cap, 0.0f);
        l.mask = cap - 1;
        l.w = 0;
        l.lp = 0;
        l.lf = 0;
        l.g = (float) std::pow (10.0, -3.0 * l.len / ((double) decaySec * sr));
        l.phase = 2 * pi * i / N;
        l.inc = 2 * pi * (0.07 + 0.055 * i) / sr;
    }
    {
        static const double msL[4] = { 2.3, 7.0, 10.9, 16.1 }, msR[4] = { 2.9, 8.1, 11.7, 17.9 };
        static const float gain[4] = { 0.65f, 0.65f, 0.6f, 0.6f };
        for (size_t i = 0; i < 4; ++i)
        {
            diffL[i].init ((size_t) std::lround (msL[i] * 0.001 * sr), gain[i]);
            diffR[i].init ((size_t) std::lround (msR[i] * 0.001 * sr), gain[i]);
        }
    }
    preLen = (size_t) std::lround (std::max (0.0f, predelaySec) * sr);
    preL.assign (preLen + 1, 0.0f);
    preR.assign (preLen + 1, 0.0f);
    preW = 0;
}

void FdnReverb::process (const float* inL, const float* inR, float* outL, float* outR, size_t n)
{
    constexpr double pi2 = 6.28318530717958647692;
    const float inGain = 0.35f;
    const float norm = 0.35355339f; // 1/sqrt(8)
    for (size_t t = 0; t < n; ++t)
    {
        float xl = inL[t], xr = inR[t];
        if (preLen > 0)
        {
            const float dl = preL[preW], dr = preR[preW];
            preL[preW] = xl; preR[preW] = xr;
            preW = (preW + 1) % preL.size();
            xl = dl; xr = dr;
        }

        for (auto& a : diffL) xl = a.process (xl);
        for (auto& a : diffR) xr = a.process (xr);

        float s[N], f[N];
        for (int i = 0; i < N; ++i)
        {
            auto& l = lines[(size_t) i];
            const double d = l.len + modDepth * std::sin (l.phase);
            l.phase += l.inc;
            if (l.phase > pi2) l.phase -= pi2;
            const double rp = (double) l.w + (double) l.buf.size() - d;
            const size_t i0 = (size_t) rp;
            const float fr = (float) (rp - (double) i0);
            s[i] = l.buf[i0 & l.mask] * (1.0f - fr) + l.buf[(i0 + 1) & l.mask] * fr;

            l.lp = (1.0f - damp) * s[i] + damp * l.lp;
            if (std::fabs (l.lp) < 1.0e-18f) l.lp = 0.0f;
            l.lf = (1.0f - lfCoef) * l.lp + lfCoef * l.lf;
            if (std::fabs (l.lf) < 1.0e-18f) l.lf = 0.0f;
            f[i] = (l.lp - lowK * l.lf) * l.g;
        }

        // Fast Walsh-Hadamard transform (orthogonal when scaled by 1/sqrt(8))
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = f[j], b = f[j + h];
                    f[j] = a + b;
                    f[j + h] = a - b;
                }

        for (int i = 0; i < N; ++i)
        {
            auto& l = lines[(size_t) i];
            const float in = (i & 1) ? xr : xl;
            l.buf[l.w] = f[i] * norm + in * inGain;
            l.w = (l.w + 1) & l.mask;
        }

        outL[t] = 0.5f * (s[0] - s[2] + s[4] - s[6]);
        outR[t] = 0.5f * (s[1] - s[3] + s[5] - s[7]);
    }
}
} // namespace jmc

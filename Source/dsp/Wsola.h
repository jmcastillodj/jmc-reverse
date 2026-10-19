#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace jmc
{
// Waveform-similarity overlap-add time stretch (two channels, correlation on the mono sum).
// Output length is exactly outLen. Offline.
inline bool wsolaStretch (const std::vector<float>& inL, const std::vector<float>& inR, size_t outLen, double sr,
                          std::vector<float>& outL, std::vector<float>& outR,
                          const std::function<bool()>& cancel)
{
    const long len = (long) inL.size();
    int N = 256;
    while (N < 0.04 * sr) N <<= 1;
    const long Hs = N / 2;
    const long tol = N / 4;
    const double ratio = (double) outLen / (double) len;
    const double Ha = (double) Hs / ratio;
    const long dstep = std::max<long> (2, (long) std::lround (sr / 24000.0)); // correlation sampling stride
    const long cstep = std::max<long> (1, (long) std::lround (sr / 48000.0)); // coarse search grid
    constexpr double pi = 3.14159265358979323846;

    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2 * pi * i / N);

    std::vector<float> mono ((size_t) len);
    std::vector<double> pref ((size_t) len + 1, 0.0);
    for (long i = 0; i < len; ++i)
    {
        mono[(size_t) i] = inL[(size_t) i] + inR[(size_t) i];
        pref[(size_t) i + 1] = pref[(size_t) i] + (double) mono[(size_t) i] * mono[(size_t) i];
    }
    auto at = [&] (const std::vector<float>& v, long i) { return (i >= 0 && i < len) ? v[(size_t) i] : 0.0f; };

    const long bufLen = (long) outLen + 2 * N;
    std::vector<float> accL ((size_t) bufLen, 0.0f), accR ((size_t) bufLen, 0.0f);
    const long maxStart = std::max<long> (0, len - N);
    long prev = 0;

    for (long k = 0;; ++k)
    {
        const long outPos = k * Hs - Hs;
        if (outPos >= (long) outLen) break;
        if ((k & 15) == 0 && cancel && cancel()) return false;

        const long nominal = std::clamp<long> ((long) std::llround ((double) outPos * Ha / (double) Hs), 0, maxStart);
        long best = nominal;
        if (k > 0)
        {
            const long tgt = prev + Hs;
            const long lo = std::max<long> (0, nominal - tol), hi = std::min<long> (maxStart, nominal + tol);
            auto scoreAt = [&] (long c)
            {
                double dot = 0;
                if (c >= 0 && tgt >= 0 && c + Hs <= len && tgt + Hs <= len)
                {
                    const float* a = mono.data() + c;
                    const float* b = mono.data() + tgt;
                    for (long j = 0; j < Hs; j += dstep)
                        dot += (double) a[j] * (double) b[j];
                }
                else
                    for (long j = 0; j < Hs; j += dstep)
                        dot += (double) at (mono, c + j) * (double) at (mono, tgt + j);
                const long e0 = std::clamp<long> (c, 0, len), e1 = std::clamp<long> (c + Hs, 0, len);
                return dot - 0.5 * (pref[(size_t) e1] - pref[(size_t) e0]) / (double) dstep; // sampled energy
            };
            // Coarse search on a sparse grid, then a fine search around the best coarse hit.
            double bestScore = -1.0e300;
            for (long c = lo; c <= hi; c += cstep)
            {
                const double sc = scoreAt (c);
                if (sc > bestScore) { bestScore = sc; best = c; }
            }
            if (cstep > 1)
                for (long c = std::max (lo, best - cstep + 1); c <= std::min (hi, best + cstep - 1); ++c)
                {
                    const double sc = scoreAt (c);
                    if (sc > bestScore) { bestScore = sc; best = c; }
                }
        }
        prev = best;
        for (long j = 0; j < N; ++j)
        {
            const long o = outPos + j;
            if (o < 0 || o >= bufLen) continue;
            accL[(size_t) o] += at (inL, best + j) * win[(size_t) j];
            accR[(size_t) o] += at (inR, best + j) * win[(size_t) j];
        }
    }
    outL.assign (accL.begin(), accL.begin() + (long) outLen);
    outR.assign (accR.begin(), accR.begin() + (long) outLen);
    return true;
}
} // namespace jmc

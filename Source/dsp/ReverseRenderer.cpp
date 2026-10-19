#include "RenderSettings.h"
#include "Biquad.h"
#include "FdnReverb.h"
#include "Wsola.h"
#include "Effects.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace jmc
{
namespace
{
float peakOf (const std::vector<float>& a, const std::vector<float>& b)
{
    float p = 0;
    for (float v : a) p = std::max (p, std::fabs (v));
    for (float v : b) p = std::max (p, std::fabs (v));
    return p;
}
} // namespace

RenderResult renderReverseReverb (const float* const* in, int numCh, size_t n, double sr,
                                  const RenderSettings& s, const std::function<bool()>& cancel, RenderCache* cache)
{
    RenderResult r;
    auto isCancelled = [&] { return cancel && cancel(); };
    if (n == 0 || sr <= 0 || numCh < 1 || in == nullptr) return r;

    // 1. Trim
    size_t a = (size_t) std::floor (std::clamp (s.trimStart, 0.0f, 1.0f) * (float) n);
    size_t b = (size_t) std::ceil (std::clamp (s.trimEnd, 0.0f, 1.0f) * (float) n);
    if (a >= n) a = n - 1;
    b = std::clamp<size_t> (b, a + 1, n);
    const size_t m = b - a;

    std::vector<float> X[2];
    for (int c = 0; c < 2; ++c)
    {
        const float* src = in[std::min (c, numCh - 1)];
        X[c].assign (src + a, src + b);
        for (auto& v : X[c]) if (! std::isfinite (v)) v = 0.0f;
    }
    const float srcPeak = peakOf (X[0], X[1]);
    const float D = s.durationSec;
    const size_t targetLen = D > 0 ? std::max<size_t> (64, (size_t) std::llround ((double) D * sr)) : 0;

    // Silent source -> silent result of the requested length, no processing.
    if (srcPeak < 1.0e-9f)
    {
        const size_t L = targetLen > 0 ? targetLen : (size_t) (0.25 * sr);
        r.ch[0].assign (L, 0.0f); r.ch[1].assign (L, 0.0f);
        r.riserSamples = L; r.ok = true;
        return r;
    }
    std::vector<float> dry[2], fwd[2], riser[2];
    const size_t tailN = (size_t) std::llround (std::clamp (s.tailSec, 0.0f, 30.0f) * sr);
    float decay = std::clamp (s.decaySec, 0.1f, 30.0f);
    if (D > 0) decay = std::clamp (decay, D * 0.25f, D * 3.0f);
    const bool tailMode = s.mode == 0;
    const size_t preSamples = (size_t) std::lround (std::max (0.0f, s.predelayMs) * 0.001 * sr);
    // Smooth-tail mode: the pre-delay becomes a silent gap between the end of the swell and the source.
    const size_t gap = (tailMode && targetLen > 0) ? std::min (preSamples, targetLen / 2) : 0;

    RenderCacheKey key;
    if (cache != nullptr)
    {
        key.srcPtr = in[0]; key.n = n; key.sr = sr; key.channels = numCh;
        unsigned long long h = 1469598103934665603ULL;
        for (int c = 0; c < 2; ++c)
            for (size_t i = 0; i < 512; ++i)
            {
                const float v = X[c][i * (m - 1) / 511];
                unsigned bits; std::memcpy (&bits, &v, sizeof bits);
                h = (h ^ bits) * 1099511628211ULL;
            }
        key.fingerprint = h ^ (unsigned long long) m;
        key.trimStart = s.trimStart; key.trimEnd = s.trimEnd; key.duration = s.durationSec; key.decay = decay; key.size = s.size;
        key.damping = s.damping; key.lowDamp = s.lowDamp; key.predelay = s.predelayMs; key.lowCut = s.lowCutHz; key.highCut = s.highCutHz;
        key.sibAmount = s.sibRegions.empty() ? 0.0f : s.sibAmount; key.pitch = s.pitchSemis; key.tail = tailN > 0; key.mode = s.mode;
        if (key.sibAmount > 0.0f) key.regions = s.sibRegions;
    }
    constexpr size_t block = 1 << 14;
    const bool hit = cache != nullptr && cache->valid && cache->key == key;
    if (hit)
    {
        for (int c = 0; c < 2; ++c) { riser[c] = cache->riser[c]; dry[c] = cache->dry[c]; fwd[c] = cache->fwd[c]; }
    }
    else
    {
        dry[0] = X[0]; dry[1] = X[1];
        // 2. Pre-filter
        for (auto& ch : X)
        {
            if (s.lowCutHz > 25.0f)  { auto q = Biquad::make (true,  s.lowCutHz,  sr); q.process (ch); }
            if (s.highCutHz < 19000.0f) { auto q = Biquad::make (false, s.highCutHz, sr); q.process (ch); }
        }
        if (tailN > 0) { fwd[0] = X[0]; fwd[1] = X[1]; }

        // 2b. Sibilance emphasis: brighten/boost detected regions and tuck the rest, so the
        //     swell is built mainly from the articulated consonants.
        if (s.sibAmount > 0.0f && ! s.sibRegions.empty())
        {
            std::vector<float> mask (m, 0.0f);
            bool any = false;
            for (const auto& g : s.sibRegions)
            {
                const long i0 = std::clamp<long> (std::lround ((double) g.start * (double) n) - (long) a, 0, (long) m);
                const long i1 = std::clamp<long> (std::lround ((double) g.end * (double) n) - (long) a, 0, (long) m);
                if (i1 > i0) { std::fill (mask.begin() + i0, mask.begin() + i1, 1.0f); any = true; }
            }
            if (any)
            {
                const long hw = std::max<long> (1, (long) (0.005 * sr)); // ~10 ms box smoothing, twice
                for (int pass = 0; pass < 2; ++pass)
                {
                    std::vector<double> pre (m + 1, 0.0);
                    for (size_t i = 0; i < m; ++i) pre[i + 1] = pre[i] + (double) mask[i];
                    for (long i = 0; i < (long) m; ++i)
                    {
                        const long lo = std::max<long> (0, i - hw), hi = std::min<long> ((long) m, i + hw + 1);
                        mask[(size_t) i] = (float) ((pre[(size_t) hi] - pre[(size_t) lo]) / (double) (hi - lo));
                    }
                }
                const float amt = std::clamp (s.sibAmount, 0.0f, 1.0f);
                for (auto& ch : X)
                {
                    std::vector<float> bright = ch;
                    auto h1 = Biquad::make (true, 4000.0, sr), h2 = Biquad::make (true, 4000.0, sr);
                    h1.process (bright);
                    h2.process (bright);
                    for (size_t i = 0; i < m; ++i)
                        ch[i] = ch[i] * (1.0f - 0.7f * amt * (1.0f - mask[i])) + bright[i] * mask[i] * 2.0f * amt;
                }
            }
        }
        if (isCancelled()) { r.cancelled = true; return r; }

        // 3. Phrase mode: reverse -> reverb -> reverse (the reversed reverb of the whole source builds
        //    into every onset). Smooth-tail mode: reverb forward and keep only what rings *after* the
        //    source, reversed - one continuous swell whatever the phrase looks like.
        if (! tailMode)
            for (auto& ch : X) std::reverse (ch.begin(), ch.end());

        const size_t tail = std::max<size_t> ((size_t) (0.1 * sr), (size_t) std::ceil (1.4 * decay * sr)) + preSamples;
        const size_t total = m + tail;
        std::vector<float> iL (total, 0.0f), iR (total, 0.0f), wL (total), wR (total);
        std::copy (X[0].begin(), X[0].end(), iL.begin());
        std::copy (X[1].begin(), X[1].end(), iR.begin());

        FdnReverb fdn;
        fdn.prepare (sr, decay, s.size, s.damping, s.predelayMs * 0.001f, s.lowDamp);
        for (size_t pos = 0; pos < total; pos += block)
        {
            if (isCancelled()) { r.cancelled = true; return r; }
            const size_t cnt = std::min (block, total - pos);
            fdn.process (iL.data() + pos, iR.data() + pos, wL.data() + pos, wR.data() + pos, cnt);
        }

        // Trim the inaudible end of the tail (-80 dB re peak) so no silence precedes the swell.
        const float wetPeak = peakOf (wL, wR);
        if (wetPeak < 1.0e-12f) { r.ok = true; r.ch[0].assign (std::max<size_t> (targetLen, 64), 0.0f); r.ch[1] = r.ch[0]; return r; }
        size_t W = total;
        const float thr = wetPeak * 1.0e-4f;
        while (W > m && std::fabs (wL[W - 1]) < thr && std::fabs (wR[W - 1]) < thr) --W;
        wL.resize (W); wR.resize (W);
        size_t T = W - m;
        if (tailMode)
        {
            // riser = reverse of the ringing tail that follows the source
            riser[0].assign (wL.rbegin(), wL.rbegin() + (long) T);
            riser[1].assign (wR.rbegin(), wR.rbegin() + (long) T);
        }
        else
        {
            std::reverse (wL.begin(), wL.end());
            std::reverse (wR.begin(), wR.end());
            // 4. Riser = everything before the source onset.
            riser[0].assign (wL.begin(), wL.begin() + (long) T);
            riser[1].assign (wR.begin(), wR.begin() + (long) T);
        }
        if (T < 2048)
        {
            for (auto& ch : riser) ch.insert (ch.begin(), 2048 - T, 0.0f);
            T = 2048;
        }

        // 5. Time-stretch to the requested duration. With a pitch glide the swell is stretched to
        //    the length the glide will consume, then read back at a varying rate, so the final
        //    length is exact and the tempo grid is preserved.
        const double semis = std::clamp ((double) s.pitchSemis, -12.0, 12.0);
        const bool glide = std::fabs (semis) > 0.01;
        const size_t L = targetLen > 0 ? std::max<size_t> (64, targetLen - gap) : T;
        std::vector<double> rates;
        const double need = glide ? fx::glideRates (rates, L, semis) : (double) L;
        const size_t stretchTo = std::max<size_t> (64, (size_t) std::llround (need));
        if ((targetLen > 0 || glide) && std::llabs ((long long) stretchTo - (long long) T) > (long long) (0.01 * (double) T))
        {
            std::vector<float> oL, oR;
            if (! wsolaStretch (riser[0], riser[1], stretchTo, sr, oL, oR, cancel)) { r.cancelled = true; return r; }
            riser[0].swap (oL);
            riser[1].swap (oR);
        }
        if (glide)
        {
            const double scale = (double) (riser[0].size() - 1) / need;
            for (auto& ch : riser) ch = fx::glideResample (ch, rates, scale);
        }

        if (cache != nullptr)
        {
            cache->key = key;
            for (int c = 0; c < 2; ++c) { cache->riser[c] = riser[c]; cache->dry[c] = dry[c]; cache->fwd[c] = fwd[c]; }
            cache->valid = true;
        }
    }
    const size_t R0 = riser[0].size();

    // 5b. Tone, modulation, stutter, saturation
    fx::tilt (riser[0], riser[1], s.tiltDb, sr);
    fx::modulate (riser[0], riser[1], s.modDepth, s.modRate, sr);
    fx::tremolo (riser[0], riser[1], s.tremMix, (double) s.tremRate, (double) s.bpm, sr);
    fx::rampTremolo (riser[0], riser[1], s.rampMix, s.rampA, s.rampB, s.rampC, (double) s.bpm, sr);
    fx::stutter (riser[0], riser[1], s.gateRate, s.gateDepth, s.gateRamp, (double) s.bpm, sr);
    fx::saturate (riser[0], riser[1], s.drive);
    if (isCancelled()) { r.cancelled = true; return r; }

    // 6. Envelope (selectable fade curve, minimum 2 ms edge smoothing)
    {
        const size_t minEdge = (size_t) (0.002 * sr);
        const size_t fi = std::min (R0, std::max (minEdge, (size_t) (std::max (0.0f, s.fadeInSec) * sr)));
        const size_t fo = std::min (R0, std::max (minEdge, (size_t) (std::max (0.0f, s.fadeOutSec) * sr)));
        for (int c = 0; c < 2; ++c)
        {
            for (size_t i = 0; i < fi; ++i)
                riser[c][i] *= (float) fx::fadeShape (s.curve, (double) i / (double) fi);
            for (size_t i = 0; i < fo; ++i)
                riser[c][R0 - 1 - i] *= (float) fx::fadeShape (s.curve, (double) i / (double) fo);
            riser[c].insert (riser[c].end(), gap, 0.0f);
        }
    }
    const size_t R = riser[0].size();

    // 7. Stereo width (mono-safe) and balance
    r.effectiveWidth = (float) fx::widen (riser[0], riser[1], s.width, s.balance);

    // 8. Level: match the source peak (only scales the swell), then assemble.
    if (s.autoLevel)
    {
        const float p = peakOf (riser[0], riser[1]);
        if (p > 1.0e-12f)
        {
            const float g = srcPeak / p;
            for (auto& ch : riser) for (auto& v : ch) v *= g;
        }
    }
    const bool withDry = s.dryLevel > 0.0f;

    // Forward reverb tail of the source, starting at the onset (optional).
    std::vector<float> tailW[2];
    if (tailN > 0)
    {
        std::vector<float> fL (tailN, 0.0f), fR (tailN, 0.0f);
        std::copy (fwd[0].begin(), fwd[0].begin() + (long) std::min (m, tailN), fL.begin());
        std::copy (fwd[1].begin(), fwd[1].begin() + (long) std::min (m, tailN), fR.begin());
        tailW[0].assign (tailN, 0.0f);
        tailW[1].assign (tailN, 0.0f);
        FdnReverb tfdn;
        tfdn.prepare (sr, decay, s.size, s.damping, s.predelayMs * 0.001f, s.lowDamp);
        for (size_t pos = 0; pos < tailN; pos += block)
        {
            if (isCancelled()) { r.cancelled = true; return r; }
            const size_t cnt = std::min (block, tailN - pos);
            tfdn.process (fL.data() + pos, fR.data() + pos, tailW[0].data() + pos, tailW[1].data() + pos, cnt);
        }
        if (s.autoLevel)
        {
            const float p = peakOf (tailW[0], tailW[1]);
            if (p > 1.0e-12f)
                for (auto& ch : tailW) for (auto& v : ch) v *= 0.5f * srcPeak / p;
        }
        const size_t fo = std::min (tailN / 2, (size_t) (0.1 * sr));
        const size_t fi = std::min (tailN, (size_t) 24);
        constexpr double pi = 3.14159265358979323846;
        for (auto& ch : tailW)
        {
            for (size_t i = 0; i < fo; ++i)
                ch[tailN - 1 - i] *= (float) (0.5 - 0.5 * std::cos (pi * (double) i / (double) fo));
            for (size_t i = 0; i < fi; ++i)
                ch[i] *= (float) i / (float) fi;
        }
    }

    const size_t pre = (size_t) std::llround (std::clamp (s.preRollSec, 0.0f, 30.0f) * sr);
    const size_t post = std::max (withDry ? m : (size_t) 0, tailN);
    r.riserSamples = R;
    r.onsetSamples = pre + R;
    for (int c = 0; c < 2; ++c)
    {
        r.ch[c].assign (pre + R + post, 0.0f);
        std::copy (riser[c].begin(), riser[c].end(), r.ch[c].begin() + (long) pre);
        float* o = r.ch[c].data() + pre + R;
        if (withDry)
        {
            const size_t ramp = std::min<size_t> (24, m);
            for (size_t i = 0; i < m; ++i)
            {
                float v = dry[c][i] * s.dryLevel;
                if (i < ramp) v *= (float) i / (float) ramp;
                o[i] += v;
            }
        }
        for (size_t i = 0; i < tailN; ++i)
            o[i] += tailW[c][i];
    }

    // 9. Output gain + peak protection (scale only, never limits or clips).
    const float gain = std::pow (10.0f, s.outGainDb / 20.0f);
    float peak = 0;
    for (auto& ch : r.ch)
        for (auto& v : ch) { v *= gain; peak = std::max (peak, std::fabs (v)); }
    if (peak > 0.989f)
    {
        const float g = 0.989f / peak;
        for (auto& ch : r.ch) for (auto& v : ch) v *= g;
    }
    fx::monoStats (r.ch[0], r.ch[1], r.correlation, r.monoLossDb);
    r.ok = true;
    return r;
}
} // namespace jmc

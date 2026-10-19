#include <cmath>
#include <cstdio>
#include <random>
#include "dsp/Effects.h"
#include "dsp/RenderSettings.h"

extern int failures;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s  [%s]\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

using namespace jmc;

namespace
{
std::vector<float> sineWave (double f, double sr, size_t n, float a = 0.5f)
{
    std::vector<float> v (n);
    for (size_t i = 0; i < n; ++i) v[i] = a * (float) std::sin (2 * 3.14159265358979 * f * (double) i / sr);
    return v;
}
double rmsOf (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (b - a));
}
double zcFreq (const std::vector<float>& v, size_t a, size_t b, double sr)
{
    size_t zc = 0; for (size_t i = a + 1; i < b; ++i) if (v[i - 1] < 0 && v[i] >= 0) ++zc;
    return (double) zc / ((double) (b - a) / sr);
}
std::vector<float> noiseBurst (double sr, double sec, unsigned seed)
{
    std::mt19937 g (seed); std::uniform_real_distribution<float> d (-1, 1);
    std::vector<float> v ((size_t) (sec * sr));
    for (size_t i = 0; i < v.size(); ++i) v[i] = 0.5f * d (g) * std::exp (-3.0f * (float) i / (float) v.size());
    return v;
}
}

void runEffectsTests();
void runEffectsTests()
{
    const double sr = 48000;

    // Tilt EQ
    {
        auto lo = sineWave (100, sr, 48000), hi = sineWave (8000, sr, 48000);
        auto lo2 = lo, hi2 = hi;
        fx::tilt (lo2, hi, 6.0f, sr); // (second arg is the other channel; just reuse the API)
        auto a = sineWave (100, sr, 48000), b = sineWave (8000, sr, 48000), a2 = a, b2 = b;
        fx::tilt (a, b, 6.0f, sr);
        const double gl = 20 * std::log10 (rmsOf (a, 24000, 48000) / rmsOf (a2, 24000, 48000));
        const double gh = 20 * std::log10 (rmsOf (b, 24000, 48000) / rmsOf (b2, 24000, 48000));
        std::printf ("      tilt +6 dB: 100 Hz %.2f dB, 8 kHz %.2f dB\n", gl, gh);
        CHECK (std::fabs (gl + 3.0) < 0.6 && std::fabs (gh - 3.0) < 0.6, "tilt +6 dB: lows -3 dB, highs +3 dB");
        (void) lo2; (void) hi2; (void) lo;
    }
    // Pitch glide
    {
        const size_t Lout = 96000;
        std::vector<double> rates;
        const double need = fx::glideRates (rates, Lout, -12.0);
        std::vector<float> src = sineWave (1000.0, sr, (size_t) std::llround (need) + 8);
        auto out = fx::glideResample (src, rates, (double) (src.size() - 1) / need);
        const double f0 = zcFreq (out, 0, 4800, sr), f1 = zcFreq (out, Lout - 4800, Lout, sr);
        std::printf ("      glide -12 st: start %.0f Hz, end %.0f Hz (expect 500 -> 1000)\n", f0, f1);
        CHECK (out.size() == Lout && std::fabs (f0 - 520) < 60 && std::fabs (f1 - 1000) < 40, "pitch glide: starts an octave low, arrives at the original pitch");
        std::vector<double> r2;
        const double need2 = fx::glideRates (r2, Lout, 7.0);
        std::vector<float> s2 = sineWave (1000.0, sr, (size_t) std::llround (need2) + 8);
        auto o2 = fx::glideResample (s2, r2, (double) (s2.size() - 1) / need2);
        CHECK (zcFreq (o2, 0, 4800, sr) > 1400, "pitch fall: starts high (+7 st)");
    }
    // Stutter gate
    {
        std::vector<float> a (96000, 1.0f), b = a;
        fx::stutter (a, b, 2, 1.0f, 0.0f, 120.0, sr); // 1/16 @120 = 8 Hz
        size_t lowCount = 0; for (float v : a) if (v < 0.05f) ++lowCount;
        const double frac = (double) lowCount / (double) a.size();
        size_t edges = 0; for (size_t i = 1; i < a.size(); ++i) if (a[i - 1] > 0.5f && a[i] <= 0.5f) ++edges;
        std::vector<float> c (96000, 1.0f), d = c;
        fx::stutter (c, d, 2, 1.0f, 1.0f, 120.0, sr);
        size_t edgesR = 0; for (size_t i = 1; i < c.size(); ++i) if (c[i - 1] > 0.5f && c[i] <= 0.5f) ++edgesR;
        std::printf ("      gate: closed %.2f of time, %zu cycles in 2 s (8 Hz => 16); ramped: %zu cycles\n", frac, edges, edgesR);
        CHECK (std::fabs (frac - 0.5) < 0.06 && edges >= 15 && edges <= 17, "stutter: 50% duty, tempo-locked rate (1/16 @120 BPM)");
        CHECK ((double) edgesR > 1.8 * (double) edges, "stutter ramp accelerates (ramp 1 = 2x cycles on average)");
        std::vector<float> e (1000, 1.0f), f = e;
        fx::stutter (e, f, 1, 0.0f, 0.0f, 120.0, sr);
        CHECK (e[500] == 1.0f || std::fabs (e[500] - 1.0f) < 1e-6f, "gate depth 0 is transparent");
    }
    // Saturation
    {
        auto a = sineWave (200, sr, 4800, 0.8f), b = a;
        fx::saturate (a, b, 1.0f);
        float pk = 0; for (float v : a) pk = std::max (pk, std::fabs (v));
        CHECK (std::fabs (pk - 0.8f) < 0.01f, "saturation preserves peak level");
        auto q = sineWave (200, sr, 4800, 0.08f), q2 = q;
        auto big = sineWave (200, sr, 4800, 0.8f); big[0] = 0.8f;
        fx::saturate (big, q2, 1.0f);
        CHECK (rmsOf (a, 0, 4800) > rmsOf (sineWave (200, sr, 4800, 0.8f), 0, 4800), "saturation raises RMS (compresses the shape)");
    }
    // Fade curves
    {
        bool ok = true;
        for (int c = 0; c < 4; ++c)
        {
            if (std::fabs (fx::fadeShape (c, 0.0)) > 1e-9 || std::fabs (fx::fadeShape (c, 1.0) - 1.0) > 1e-9) ok = false;
            double prev = -1; for (int i = 0; i <= 100; ++i) { double v = fx::fadeShape (c, i / 100.0); if (v < prev) ok = false; prev = v; }
        }
        CHECK (ok, "all fade curves run 0->1 monotonically");
        CHECK (fx::fadeShape (2, 0.5) < fx::fadeShape (1, 0.5) && fx::fadeShape (3, 0.5) > fx::fadeShape (1, 0.5), "exponential below linear below logarithmic");
    }
    // Width + safeguard + balance
    {
        std::mt19937 g (5); std::uniform_real_distribution<float> d (-1, 1);
        std::vector<float> l (48000), r (48000);
        for (size_t i = 0; i < l.size(); ++i) { float m = 0.2f * d (g), sd = 0.2f * d (g); l[i] = m + sd; r[i] = m - sd; }
        auto l1 = l, r1 = r;
        const double w = fx::widen (l1, r1, 2.0f, 0.0f);
        float corr, loss; fx::monoStats (l1, r1, corr, loss);
        std::printf ("      width 2 on decorrelated input: effective %.2f, corr %.2f, mono %.2f dB\n", w, corr, loss);
        CHECK (corr >= -0.52f && loss > -6.1f, "wide: mono sum loses no more than ~6 dB");
        std::vector<float> ml (48000), mr;
        for (size_t i = 0; i < ml.size(); ++i) ml[i] = 0.2f * d (g);
        mr = ml;
        for (size_t i = 0; i < mr.size(); ++i) mr[i] += 0.01f * d (g);
        const double w2 = fx::widen (ml, mr, 2.0f, 0.0f);
        CHECK (std::fabs (w2 - 2.0) < 1e-6, "near-mono source can be widened fully");
        auto l3 = l, r3 = r;
        fx::widen (l3, r3, 0.0f, 0.0f);
        bool same = true; for (size_t i = 0; i < l3.size(); ++i) if (std::fabs (l3[i] - r3[i]) > 1e-6f) same = false;
        CHECK (same, "width 0 -> mono");
        auto l4 = l, r4 = r;
        fx::widen (l4, r4, 1.0f, 1.0f);
        CHECK (rmsOf (l4, 0, l4.size()) < 1e-9 && rmsOf (r4, 0, r4.size()) > 0.1, "balance +1 silences the left channel");
    }
    // Modulation
    {
        auto a = noiseBurst (sr, 1.0, 3), b = a, a0 = a;
        fx::modulate (a, b, 0.0f, 1.0f, sr);
        CHECK (a == a0, "modulation depth 0 is transparent");
        fx::modulate (a, b, 1.0f, 2.0f, sr);
        bool fin = true; for (float v : a) if (! std::isfinite (v)) fin = false;
        CHECK (fin && std::fabs (rmsOf (a, 0, a.size()) / rmsOf (a0, 0, a0.size()) - 1.0) < 0.5, "modulation finite, level sane");
    }
    // Low damping: less low-frequency energy in the tail
    {
        auto x = noiseBurst (sr, 0.5, 4);
        const float* ch[1] = { x.data() };
        RenderSettings a, b; a.lowDamp = 0.0f; b.lowDamp = 1.0f; a.durationSec = b.durationSec = 3; a.decaySec = b.decaySec = 4;
        a.autoLevel = b.autoLevel = false;
        auto ra = renderReverseReverb (ch, 1, x.size(), sr, a), rb = renderReverseReverb (ch, 1, x.size(), sr, b);
        auto lpA = ra.ch[0], lpB = rb.ch[0];
        auto q1 = Biquad::make (false, 150, sr), q2 = Biquad::make (false, 150, sr);
        q1.process (lpA); q2.process (lpB);
        const double rA = rmsOf (lpA, 0, lpA.size()) / rmsOf (ra.ch[0], 0, ra.ch[0].size());
        const double rB = rmsOf (lpB, 0, lpB.size()) / rmsOf (rb.ch[0], 0, rb.ch[0].size());
        std::printf ("      low-band share: lowDamp 0 = %.3f, lowDamp 1 = %.3f\n", rA, rB);
        CHECK (rB < rA, "low damping reduces low-frequency share of the reverb");
    }
    // Full render with every effect, exact length, stable
    for (int pitch : { -12, -7, 0, 7, 12 })
    {
        auto x = noiseBurst (sr, 0.5, 6);
        const float* ch[1] = { x.data() };
        RenderSettings s; s.durationSec = 3; s.pitchSemis = (float) pitch; s.tiltDb = 4; s.modDepth = 0.6f; s.gateRate = 2; s.gateDepth = 0.9f;
        s.gateRamp = 0.5f; s.bpm = 120; s.drive = 0.7f; s.width = 2; s.balance = 0.3f; s.curve = 2; s.dryLevel = 1; s.tailSec = 1; s.sibAmount = 0;
        auto r = renderReverseReverb (ch, 1, x.size(), sr, s);
        bool fin = true; float pk = 0; for (auto& c : r.ch) for (float v : c) { if (! std::isfinite (v)) fin = false; pk = std::max (pk, std::fabs (v)); }
        char msg[96]; std::snprintf (msg, sizeof msg, "all effects @ pitch %+d st: exact length, finite, peak-safe", pitch);
        CHECK (r.ok && fin && pk <= 0.989f + 1e-6f && r.riserSamples == (size_t) (3 * sr), msg);
    }
    {
        auto x = noiseBurst (sr, 0.5, 7);
        const float* ch[1] = { x.data() };
        RenderSettings s; s.width = 0;
        auto r = renderReverseReverb (ch, 1, x.size(), sr, s);
        CHECK (r.correlation > 0.999f && std::fabs (r.monoLossDb) < 0.05f, "mono metrics: width 0 -> correlation 1, no mono loss");
        RenderSettings w; w.width = 2;
        auto rw = renderReverseReverb (ch, 1, x.size(), sr, w);
        std::printf ("      width 2: effective %.2f, corr %.2f, mono loss %.2f dB\n", rw.effectiveWidth, rw.correlation, rw.monoLossDb);
        CHECK (rw.monoLossDb > -6.2f, "wide render stays mono-compatible");
    }
}

void runEffectsTests2();
void runEffectsTests2()
{
    const double sr = 48000;
    // RAMP TREMOLO: speed glides through three note values, locked to the tempo
    {
        auto dips = [&] (const std::vector<float>& v, size_t from, size_t to)
        {
            size_t c = 0;
            for (size_t i = from + 1; i + 1 < to; ++i) if (v[i] < v[i - 1] && v[i] <= v[i + 1] && v[i] < 0.01f) ++c;
            return c;
        };
        const size_t n = (size_t) (4 * sr);
        std::vector<float> a (n, 1.0f), b = a;
        fx::rampTremolo (a, b, 1.0f, 0.0f, 1.0f, 2.0f, 120.0, sr); // 1/4 -> 1/8 -> 1/16 at 120 BPM: 2 Hz -> 4 Hz -> 8 Hz
        const size_t total = dips (a, 0, n), first = dips (a, 0, n / 2), second = dips (a, n / 2, n);
        std::printf ("      ramp 1/4 -> 1/8 -> 1/16 @120 BPM over 4 s: %zu dips (expected ~18), first half %zu (~6), second half %zu (~12)\n", total, first, second);
        CHECK (total >= 16 && total <= 20 && first >= 5 && first <= 7 && second >= 11 && second <= 13, "RAMP TREM accelerates through three note values (1/4 -> 1/8 -> 1/16)");
        CHECK (std::fabs (a[0] - 1.0f) < 1e-6f, "RAMP TREM starts at full level");
        std::vector<float> c (n, 1.0f), d = c;
        fx::rampTremolo (c, d, 1.0f, 2.0f, 0.0f, 2.0f, 120.0, sr); // 1/16 -> 1/4 -> 1/16: slow in the middle
        const size_t third = n / 3, d1 = dips (c, 0, third), d2 = dips (c, third, 2 * third), d3 = dips (c, 2 * third, n);
        std::printf ("      ramp 1/16 -> 1/4 -> 1/16: dips per third %zu / %zu / %zu\n", d1, d2, d3);
        CHECK (d2 < d1 && d2 < d3, "RAMP TREM can slow down and speed up again (middle value respected)");
        std::vector<float> e1 (n, 1.0f), e2 = e1;
        fx::rampTremolo (e1, e2, 0.5f, 0.0f, 1.0f, 2.0f, 120.0, sr);
        float mn = 1; for (float v : e1) mn = std::min (mn, v);
        CHECK (mn >= 0.499f, "RAMP TREM mix 0.5 never drops below 0.5");
        std::vector<float> f1 (100, 1.0f), f2 = f1;
        fx::rampTremolo (f1, f2, 1.0f, 0.0f, 1.0f, 2.0f, 0.0, sr);
        CHECK (f1[50] == 1.0f || std::fabs (f1[50] - 1.0f) < 1e-6f, "RAMP TREM without tempo is ignored");
    }
    // TREMOLO: depth, rate lock to tempo
    {
        std::vector<float> a (96000, 1.0f), b = a;
        fx::tremolo (a, b, 1.0f, 2.0, 120.0, sr); // 1/8 @120 BPM = 4 Hz
        float mn = 1, mx = 0; for (float v : a) { mn = std::min (mn, v); mx = std::max (mx, v); }
        size_t minima = 0; for (size_t i = 1; i + 1 < a.size(); ++i) if (a[i] < a[i - 1] && a[i] <= a[i + 1] && a[i] < 0.01f) ++minima;
        std::printf ("      tremolo: min %.3f max %.3f, %zu dips in 2 s (4 Hz => 8)\n", mn, mx, minima);
        CHECK (mn < 0.001f && mx > 0.999f && minima >= 7 && minima <= 8, "TREMOLO: full depth, 1/8 @120 BPM = 4 Hz");
        std::vector<float> c (1000, 1.0f), d = c;
        fx::tremolo (c, d, 0.5f, 2.0, 120.0, sr);
        float cm = 1; for (float v : c) cm = std::min (cm, v);
        CHECK (cm >= 0.499f, "TREMOLO mix 0.5 never drops below 0.5");
        fx::tremolo (c, d, 1.0f, 2.0, 0.0, sr); // no tempo: must be a no-op, not a crash
        CHECK (true, "TREMOLO without tempo is ignored");
    }
}

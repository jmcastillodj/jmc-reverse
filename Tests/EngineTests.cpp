#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include "dsp/RenderSettings.h"
#include "dsp/Wsola.h"
#include "dsp/Timing.h"
#include "dsp/FdnReverb.h"

extern int failures;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s  [%s]\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

using namespace jmc;

static std::vector<float> burst (double sr, double seconds, float amp, unsigned seed = 1)
{
    // decaying noise burst: a stand-in for a short vocal phrase
    std::mt19937 g (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    std::vector<float> v ((size_t) (seconds * sr));
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = amp * d (g) * std::exp (-3.0f * (float) i / (float) v.size());
    return v;
}

static RenderResult run (const std::vector<float>& l, const std::vector<float>* r, double sr, const RenderSettings& s)
{
    const float* ch[2] = { l.data(), r ? r->data() : l.data() };
    return renderReverseReverb (ch, r ? 2 : 1, l.size(), sr, s);
}

static bool allFinite (const RenderResult& r)
{
    for (auto& c : r.ch) for (float v : c) if (! std::isfinite (v)) return false;
    return true;
}
static float peak (const RenderResult& r)
{
    float p = 0;
    for (auto& c : r.ch) for (float v : c) p = std::max (p, std::fabs (v));
    return p;
}
static double rmsRange (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += double (v[i]) * v[i];
    return std::sqrt (s / double (b - a));
}

void runEngineTests();
void runEngineTests()
{
    const double sr = 48000;
    RenderSettings s;

    // Silence
    {
        std::vector<float> z (48000, 0.0f);
        auto r = run (z, nullptr, sr, s);
        CHECK (r.ok && allFinite (r) && peak (r) == 0.0f && r.length() == (size_t) (4 * sr), "silence -> silent output of requested length");
    }
    // Basic swell structure
    {
        auto x = burst (sr, 0.6, 0.5f);
        auto t0 = std::chrono::steady_clock::now();
        auto r = run (x, nullptr, sr, s);
        double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("      render time (0.6 s src, 4 s riser): %.3f s\n", secs);
        CHECK (r.ok && allFinite (r), "burst renders, finite");
        CHECK (r.length() == (size_t) (4 * sr), "length == requested 4 s");
        CHECK (peak (r) <= 0.989f + 1e-6f, "peak protected");
        CHECK (std::fabs (peak (r) - 0.5f) < 0.02f, "auto level matches source peak");
        const size_t L = r.length();
        double early = rmsRange (r.ch[0], L / 10, 2 * L / 10), late = rmsRange (r.ch[0], 8 * L / 10, 9 * L / 10);
        std::printf ("      early %.5f  late %.5f  (%.1f dB)\n", early, late, 20 * std::log10 (late / early));
        CHECK (late > early * 4.0, "swell builds (late window >12 dB above early)");
        CHECK (std::fabs (r.ch[0][0]) < 1e-4f && std::fabs (r.ch[0][L - 1]) < 1e-3f, "starts and ends at ~zero (no clicks at edges)");
        double d = rmsRange (r.ch[0], 0, L) ; (void) d;
        CHECK (rmsRange (r.ch[0], L / 2, L) > 0 && rmsRange (r.ch[1], L / 2, L) > 0, "stereo output from mono input");
        double corr = 0, e0 = 0, e1 = 0;
        for (size_t i = L / 2; i < L; ++i) { corr += double (r.ch[0][i]) * r.ch[1][i]; e0 += double (r.ch[0][i]) * r.ch[0][i]; e1 += double (r.ch[1][i]) * r.ch[1][i]; }
        std::printf ("      L/R correlation: %.3f\n", corr / std::sqrt (e0 * e1));
        CHECK (corr / std::sqrt (e0 * e1) < 0.95, "channels decorrelated (stereo swell)");
    }
    // Durations
    for (float D : { 0.5f, 1.0f, 4.0f, 16.0f })
    {
        auto x = burst (sr, 0.4, 0.3f, 2);
        RenderSettings q; q.durationSec = D;
        auto r = run (x, nullptr, sr, q);
        char msg[96]; std::snprintf (msg, sizeof msg, "duration %.1f s honoured, finite, peak ok", D);
        CHECK (r.ok && allFinite (r) && r.length() == (size_t) std::llround (D * sr) && peak (r) <= 0.989f + 1e-6f, msg);
    }
    // Natural length (no stretch)
    {
        auto x = burst (sr, 0.4, 0.3f, 3);
        RenderSettings q; q.durationSec = 0;
        auto r = run (x, nullptr, sr, q);
        std::printf ("      natural length: %.2f s\n", (double) r.length() / sr);
        CHECK (r.ok && (double) r.length() > 0.5 * sr, "natural length mode");
    }
    // Sample rates and extreme reverb
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        auto x = burst (rate, 0.3, 0.9f, 4);
        RenderSettings q; q.decaySec = 20; q.size = 2; q.damping = 0; q.durationSec = 6;
        auto r = run (x, nullptr, rate, q);
        char msg[96]; std::snprintf (msg, sizeof msg, "extreme reverb @ %.0f Hz stable", rate);
        CHECK (r.ok && allFinite (r) && peak (r) <= 0.989f + 1e-6f && r.length() == (size_t) std::llround (6 * rate), msg);
    }
    {
        auto x = burst (sr, 0.3, 0.9f, 5);
        RenderSettings q; q.decaySec = 0.1f; q.size = 0.25f; q.damping = 1; q.durationSec = 0.2f;
        auto r = run (x, nullptr, sr, q);
        CHECK (r.ok && allFinite (r), "minimum reverb settings stable");
    }
    // Very short / quiet / loud sources
    {
        std::vector<float> one (1, 1.0f);
        auto r = run (one, nullptr, sr, s);
        CHECK (r.ok && allFinite (r) && peak (r) <= 0.989f + 1e-6f, "1-sample source");
        std::vector<float> quiet = burst (sr, 0.5, 1e-6f, 6);
        auto rq = run (quiet, nullptr, sr, s);
        CHECK (rq.ok && allFinite (rq) && peak (rq) > 0 && peak (rq) < 1e-5f, "extremely quiet source stays quiet & finite");
        std::vector<float> sq (24000);
        for (size_t i = 0; i < sq.size(); ++i) sq[i] = (i / 100) % 2 ? 1.0f : -1.0f;
        auto rl = run (sq, nullptr, sr, s);
        CHECK (rl.ok && allFinite (rl) && peak (rl) <= 0.989f + 1e-6f, "full-scale square wave source");
    }
    // Stereo input, dry append, join continuity
    {
        auto l = burst (sr, 0.5, 0.5f, 7), rr = burst (sr, 0.5, 0.5f, 8);
        RenderSettings q; q.dryLevel = 1.0f; q.durationSec = 3;
        auto r = run (l, &rr, sr, q);
        CHECK (r.ok && allFinite (r) && r.length() == (size_t) (3 * sr) + l.size(), "dry appended: length = riser + source");
        const size_t j = r.riserSamples;
        // The envelope forces the swell to ~0 at the join, then the source ramps in.
        CHECK (std::fabs (r.ch[0][j - 1]) < 5e-3f && std::fabs (r.ch[0][j]) < 5e-3f, "no discontinuity at riser/source join");
        CHECK (std::fabs (r.ch[0][j + 2000] - l[2000]) < 1e-6f && std::fabs (r.ch[1][j + 3000] - rr[3000]) < 1e-6f, "appended source is bit-exact (stereo preserved)");
    }
    // Repeated regeneration is deterministic
    {
        auto x = burst (sr, 0.5, 0.5f, 9);
        auto a = run (x, nullptr, sr, s), b = run (x, nullptr, sr, s);
        CHECK (a.ch[0] == b.ch[0] && a.ch[1] == b.ch[1], "repeated render is bit-identical");
    }
    // Trim
    {
        auto x = burst (sr, 1.0, 0.5f, 10);
        RenderSettings q; q.trimStart = 0.2f; q.trimEnd = 0.5f; q.dryLevel = 1; q.durationSec = 2;
        auto r = run (x, nullptr, sr, q);
        const size_t expect = (size_t) (2 * sr) + (size_t) (std::ceil (0.5 * (double) x.size()) - std::floor (0.2 * (double) x.size()));
        CHECK (r.ok && r.length() == expect, "trim region honoured");
    }
    // WSOLA: pitch (frequency) preserved, level steady, length exact
    for (double ratio : { 0.5, 2.0, 3.0 })
    {
        std::vector<float> t (48000);
        for (size_t i = 0; i < t.size(); ++i) t[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 440.0 * (double) i / sr);
        std::vector<float> oL, oR;
        const size_t outLen = (size_t) ((double) t.size() * ratio);
        bool ok = wsolaStretch (t, t, outLen, sr, oL, oR, {});
        size_t zc = 0, a = outLen / 4, b = 3 * outLen / 4;
        for (size_t i = a + 1; i < b; ++i) if (oL[i - 1] < 0 && oL[i] >= 0) ++zc;
        const double f = (double) zc / ((double) (b - a) / sr);
        const double lvl = rmsRange (oL, a, b);
        char msg[96]; std::snprintf (msg, sizeof msg, "WSOLA x%.1f keeps 440 Hz (%.1f) and level (%.3f)", ratio, f, lvl);
        CHECK (ok && oL.size() == outLen && std::fabs (f - 440.0) < 8.0 && std::fabs (lvl - 0.3536) < 0.05, msg);
    }
    // Timing helpers / tempo changes
    {
        using namespace jmc::timing;
        CHECK (std::fabs (beatsToSeconds (4, 120) - 2.0f) < 1e-6f, "4 beats @120 BPM = 2 s");
        CHECK (std::fabs (beatsToSeconds (4, 60) - 4.0f) < 1e-6f && std::fabs (beatsToSeconds (16, 90) - 10.6667f) < 1e-3f, "tempo change rescales duration");
        CHECK (std::fabs (beatsToSeconds (0.25, 300) - (float) minDuration) < 1e-6f && std::fabs (beatsToSeconds (16, 20) - (float) maxDuration) < 1e-6f, "duration clamped to sensible limits");
        CHECK (std::fabs (beatsToSeconds (4, 0) - 2.0f) < 1e-6f && std::fabs (beatsToSeconds (4, std::nan ("")) - 2.0f) < 1e-6f && std::fabs (sanitiseBpm (-5) - 120.0) < 1e-9, "invalid host tempo falls back to 120");
        CHECK (std::fabs (beatsToBars (4, 4, 4) - 1.0) < 1e-9 && std::fabs (beatsToBars (6, 6, 8) - 2.0) < 1e-9 && std::fabs (beatsToBars (3, 3, 4) - 1.0) < 1e-9, "beats -> bars for 4/4, 6/8, 3/4");
        auto x = burst (sr, 0.4, 0.3f, 13);
        RenderSettings a, b; a.durationSec = beatsToSeconds (8, 120); b.durationSec = beatsToSeconds (8, 90);
        auto ra = run (x, nullptr, sr, a), rb = run (x, nullptr, sr, b);
        CHECK (ra.length() == (size_t) std::llround (4.0 * sr) && rb.length() == (size_t) std::llround ((double) beatsToSeconds (8, 90) * sr), "render length follows tempo (120 -> 90 BPM)");
    }
    // Pre-roll and tail
    {
        auto x = burst (sr, 0.5, 0.5f, 14);
        RenderSettings q; q.durationSec = 2; q.preRollSec = 0.5f; q.tailSec = 1.5f;
        auto r = run (x, nullptr, sr, q);
        CHECK (r.ok && allFinite (r) && r.length() == (size_t) (0.5 * sr + 2 * sr + 1.5 * sr), "pre-roll + riser + tail length");
        CHECK (r.onsetSamples == (size_t) (2.5 * sr), "onset marks pre-roll + riser");
        bool silent = true; for (size_t i = 0; i < (size_t) (0.5 * sr); ++i) if (r.ch[0][i] != 0.0f) silent = false;
        CHECK (silent, "pre-roll is digital silence");
        const size_t on = r.onsetSamples;
        CHECK (rmsRange (r.ch[0], on + 2000, on + 20000) > 1e-4, "tail has audible forward reverb after the onset");
        CHECK (std::fabs (r.ch[0][r.length() - 1]) < 1e-3f && std::fabs (r.ch[0][on]) < 1e-2f, "tail ends and starts smoothly");
        CHECK (peak (r) <= 0.989f + 1e-6f, "peak protected with tail");
        RenderSettings q2 = q; q2.dryLevel = 1.0f; q2.tailSec = 0.1f;
        auto r2 = run (x, nullptr, sr, q2);
        CHECK (r2.length() == (size_t) (0.5 * sr + 2 * sr) + x.size(), "dry longer than tail dictates length");
    }
    // Render cache: identical output with or without it, faster when only downstream settings change
    {
        auto x = burst (sr, 2.0, 0.5f, 21);
        const float* ch[1] = { x.data() };
        RenderCache cache;
        RenderSettings s1; s1.durationSec = 6; s1.decaySec = 4; s1.tailSec = 1;
        RenderSettings s2 = s1; s2.tiltDb = 4; s2.width = 1.5f; s2.drive = 0.3f; s2.fadeInSec = 0.4f; s2.dryLevel = 0.8f; s2.tailSec = 2; s2.outGainDb = -3;
        RenderSettings s3 = s2; s3.decaySec = 2.0f;
        auto t0 = std::chrono::steady_clock::now();
        auto r1 = renderReverseReverb (ch, 1, x.size(), sr, s1, {}, &cache);
        auto t1 = std::chrono::steady_clock::now();
        auto r2c = renderReverseReverb (ch, 1, x.size(), sr, s2, {}, &cache);
        auto t2 = std::chrono::steady_clock::now();
        auto r2 = renderReverseReverb (ch, 1, x.size(), sr, s2);
        auto t3 = std::chrono::steady_clock::now();
        auto r3c = renderReverseReverb (ch, 1, x.size(), sr, s3, {}, &cache);
        auto r3 = renderReverseReverb (ch, 1, x.size(), sr, s3);
        const double miss = std::chrono::duration<double> (t1 - t0).count(), hitT = std::chrono::duration<double> (t2 - t1).count(), plain = std::chrono::duration<double> (t3 - t2).count();
        std::printf ("      cache: first render %.3f s, downstream-only change with cache %.3f s (without cache %.3f s)\n", miss, hitT, plain);
        CHECK (r1.ok && r2c.ch[0] == r2.ch[0] && r2c.ch[1] == r2.ch[1] && r2c.length() == r2.length(), "cached render is bit-identical to an uncached one");
        CHECK (r3c.ch[0] == r3.ch[0] && r3c.ch[1] == r3.ch[1], "changing a reverb setting invalidates the cache correctly");
        CHECK (hitT < plain * 0.7, "downstream-only changes skip the reverb (faster)");
    }
    // Smooth-tail mode: continuous swell for sparse speech-like material (no word-by-word chopping)
    {
        std::mt19937 g (5);
        std::vector<float> v ((size_t) (5.0 * sr), 0.0f);
        struct Wd { double a, b, f0; };
        for (auto& w : std::initializer_list<Wd> { { 0.10, 0.40, 170 }, { 0.90, 1.20, 190 }, { 1.90, 2.15, 160 }, { 2.70, 3.30, 180 }, { 4.20, 4.60, 200 } })
            for (size_t i = (size_t) (w.a * sr); i < (size_t) (w.b * sr); ++i)
            {
                const double t = (double) i / sr, xx = (t - w.a) / (w.b - w.a), env = std::sin (3.14159 * xx);
                double sm = 0; for (int k = 1; k <= 14; ++k) sm += std::sin (6.2831853 * w.f0 * k * t) / k;
                v[i] = (float) (0.35 * env * sm);
            }
        const float* ch[1] = { v.data() };
        for (int mode : { 0, 1 })
            for (float len : { 4.0f, 1.0f })
            {
                RenderSettings q; q.mode = mode; q.durationSec = len;
                auto r = renderReverseReverb (ch, 1, v.size(), sr, q);
                const size_t w = (size_t) (0.05 * sr);
                std::vector<double> e;
                for (size_t i = 0; i + w <= r.length(); i += w) { double acc = 0; for (size_t k = 0; k < w; ++k) acc += (double) r.ch[0][i + k] * r.ch[0][i + k]; e.push_back (10 * std::log10 (acc / (double) w + 1e-14)); }
                double worst = 0, run = -200;
                for (size_t i = 0; i < e.size(); ++i) { if (i > e.size() / 4) worst = std::min (worst, e[i] - run); run = std::max (run, e[i]); }
                char msg[128]; std::snprintf (msg, sizeof msg, "%s mode, %.0f s: swell never dips more than 3 dB (worst %.1f dB)", mode == 0 ? "smooth" : "phrase", len, worst);
                CHECK (r.ok && r.length() == (size_t) std::llround (len * sr) && worst > -3.0, msg);
            }
        RenderSettings a, b; a.mode = 0; b.mode = 1;
        auto ra = renderReverseReverb (ch, 1, v.size(), sr, a), rb = renderReverseReverb (ch, 1, v.size(), sr, b);
        double diff = 0, ea = 0; for (size_t i = 0; i < ra.length(); ++i) { const double d = ra.ch[0][i] - rb.ch[0][i]; diff += d * d; ea += (double) ra.ch[0][i] * ra.ch[0][i]; }
        CHECK (diff > 0.1 * ea, "smooth and phrase modes are genuinely different renders");
    }
    // Reverb echo density: no discrete flutter echoes after the first ~200 ms
    {
        const size_t n = (size_t) (2 * sr);
        FdnReverb f; f.prepare (sr, 2.5f, 1.0f, 0.4f, 0.0f, 0.2f);
        std::vector<float> in (n, 0.0f), l (n), r (n); in[0] = 1.0f;
        f.process (in.data(), in.data(), l.data(), r.data(), n);
        const size_t w = (size_t) (0.025 * sr);
        double worstLate = 0, early = 0;
        for (size_t t = (size_t) (0.075 * sr); t + w < n && t < (size_t) (1.5 * sr); t += w)
        {
            double m2 = 0, m4 = 0; for (size_t i = t; i < t + w; ++i) { const double x = l[i]; m2 += x * x; m4 += x * x * x * x; }
            m2 /= (double) w; m4 /= (double) w;
            const double k = m2 > 1e-30 ? m4 / (m2 * m2) : 3.0;
            if (t < (size_t) (0.2 * sr)) early = std::max (early, k); else worstLate = std::max (worstLate, k);
        }
        std::printf ("      kurtosis: first 200 ms max %.1f, later max %.1f (Gaussian = 3)\n", early, worstLate);
        CHECK (early < 12.0 && worstLate < 4.5, "reverb is dense (input diffusion): no sparse flutter echoes");
    }
    // Cancellation
    {
        auto x = burst (sr, 0.5, 0.5f, 11);
        const float* ch[1] = { x.data() };
        auto r = renderReverseReverb (ch, 1, x.size(), sr, s, [] { return true; });
        CHECK (r.cancelled && ! r.ok, "render honours cancellation");
    }
    // Filters
    {
        auto x = burst (sr, 0.5, 0.5f, 12);
        RenderSettings q; q.highCutHz = 1000; q.lowCutHz = 200;
        auto r = run (x, nullptr, sr, q);
        CHECK (r.ok && allFinite (r), "low/high cut render");
    }
}

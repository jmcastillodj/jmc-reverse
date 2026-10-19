#include <cmath>
#include <cstdio>
#include <random>
#include "dsp/Biquad.h"
#include "dsp/Sibilance.h"
#include "dsp/RenderSettings.h"

extern int failures;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s  [%s]\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

using namespace jmc;

namespace
{
std::mt19937 rng (42);
std::vector<float> noise (size_t n) { std::uniform_real_distribution<float> d (-1, 1); std::vector<float> v (n); for (auto& x : v) x = d (rng); return v; }

// Voiced "vowel": harmonic stack with vibrato-free f0 and gentle spectral tilt.
void addVowel (std::vector<float>& v, double sr, double t0, double t1, double f0, float amp)
{
    for (size_t i = (size_t) (t0 * sr); i < (size_t) (t1 * sr) && i < v.size(); ++i)
    {
        double t = (double) i / sr, s = 0;
        for (int k = 1; k <= 30; ++k) s += std::sin (2 * 3.14159265358979 * f0 * k * t) / (double) (k * k);
        v[i] += amp * (float) s;
    }
}
void addBandNoise (std::vector<float>& v, double sr, double t0, double t1, double lo, double hi, float amp, float decay = 0.0f)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (v.size(), (size_t) (t1 * sr));
    auto nz = noise (b - a);
    if (lo > 0) { auto q = Biquad::make (true, lo, sr); q.process (nz); auto q2 = Biquad::make (true, lo, sr); q2.process (nz); }
    if (hi < sr * 0.45) { auto q = Biquad::make (false, hi, sr); q.process (nz); auto q2 = Biquad::make (false, hi, sr); q2.process (nz); }
    for (size_t i = 0; i < nz.size(); ++i)
    {
        const double x = (double) i / (double) nz.size();
        float e = std::min (1.0f, (float) i / (float) (0.01 * sr)) * std::min (1.0f, (float) (nz.size() - i) / (float) (0.01 * sr));
        if (decay > 0) e = std::exp (-decay * (float) x);
        v[a + i] += amp * nz[i] * e;
    }
}
SibResult detect (const std::vector<float>& l, const std::vector<float>* r, double sr, float sens)
{
    const float* ch[2] = { l.data(), r ? r->data() : l.data() };
    auto f = analyseSibilance (ch, r ? 2 : 1, l.size(), sr);
    return detectSibilance (*f, sens);
}
bool hit (const SibResult& r, double sr, size_t n, double t0, double t1)
{
    for (auto& g : r.regions)
    {
        const double a = g.start * (double) n / sr, b = g.end * (double) n / sr;
        if (a < t1 + 0.05 && b > t0 - 0.05) return true;
    }
    return false;
}
std::vector<float> phrase (double sr)
{
    // "ah - s - ah - sh - ah": vowels with an S (5-11 kHz noise) and an SH (2.5-7 kHz noise)
    std::vector<float> v ((size_t) (2.4 * sr), 0.0f);
    addVowel (v, sr, 0.0, 0.5, 150, 0.5f);
    addBandNoise (v, sr, 0.5, 0.75, 5500, 11000, 0.8f);
    addVowel (v, sr, 0.75, 1.3, 170, 0.5f);
    addBandNoise (v, sr, 1.3, 1.6, 3500, 8000, 0.8f);
    addVowel (v, sr, 1.6, 2.4, 140, 0.5f);
    return v;
}
} // namespace

void runSibilanceTests();
void runSibilanceTests()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        char msg[128];
        auto v = phrase (sr);
        auto r = detect (v, nullptr, sr, 0.6f);
        std::snprintf (msg, sizeof msg, "@%.0f Hz: S found (%zu regions)", sr, r.regions.size());
        CHECK (r.reliable && hit (r, sr, v.size(), 0.5, 0.75), msg);
        std::snprintf (msg, sizeof msg, "@%.0f Hz: no region over vowels", sr);
        bool bad = false;
        for (auto& g : r.regions)
        {
            const double ga = g.start * (double) v.size() / sr, gb = g.end * (double) v.size() / sr;
            const bool nearS = ga < 0.81 && gb > 0.44, nearSh = ga < 1.66 && gb > 1.24;
            if (! (nearS || nearSh)) bad = true; // every region must overlap a real fricative
        }
        CHECK (! bad, msg);
    }
    {
        auto v = phrase (48000.0);
        auto lo = detect (v, nullptr, 48000.0, 0.1f), hi = detect (v, nullptr, 48000.0, 0.9f);
        std::printf ("      sens 0.1: %zu regions (cov %.3f)   sens 0.9: %zu regions (cov %.3f)\n", lo.regions.size(), lo.coverage, hi.regions.size(), hi.coverage);
        CHECK (hi.coverage >= lo.coverage, "higher sensitivity never detects less");
        CHECK (hit (hi, 48000.0, v.size(), 1.3, 1.6), "SH-like noise found at high sensitivity");
    }
    // Non-sibilant content
    {
        const double sr = 48000;
        std::vector<float> v ((size_t) (2 * sr), 0.0f);
        addVowel (v, sr, 0.0, 2.0, 150, 0.5f);
        auto r = detect (v, nullptr, sr, 0.9f);
        CHECK (r.regions.empty() && ! r.message.empty(), "vowel only: nothing detected, message given");
        std::vector<float> breath ((size_t) (1.5 * sr), 0.0f);
        addBandNoise (breath, sr, 0.3, 1.0, 300, 2500, 0.5f);
        auto rb = detect (breath, nullptr, sr, 0.9f);
        CHECK (rb.regions.empty(), "breath (300-2500 Hz noise) rejected");
        std::vector<float> hat ((size_t) (1.5 * sr), 0.0f);
        addBandNoise (hat, sr, 0.3, 0.325, 6000, 16000, 0.8f, 6.0f);
        addBandNoise (hat, sr, 0.8, 0.825, 6000, 16000, 0.8f, 6.0f);
        auto rh = detect (hat, nullptr, sr, 0.9f);
        CHECK (rh.regions.empty(), "closed hi-hat (25 ms) rejected");
        std::vector<float> wash ((size_t) (10 * sr), 0.0f);
        addBandNoise (wash, sr, 0.0, 10.0, 6000, 16000, 0.5f);
        auto rw = detect (wash, nullptr, sr, 0.9f);
        CHECK (rw.regions.empty() && rw.message.find ("throughout") != std::string::npos, "continuous HF noise flagged as non-vocal");
        std::vector<float> silence ((size_t) sr, 0.0f);
        auto rs = detect (silence, nullptr, sr, 0.9f);
        CHECK (rs.regions.empty(), "digital silence: nothing detected");
        std::vector<float> tiny (10, 0.1f);
        auto rt = detect (tiny, nullptr, sr, 0.5f);
        CHECK (rt.regions.empty(), "very short source does not crash");
    }
    // Stereo
    {
        const double sr = 48000;
        auto v = phrase (sr);
        std::vector<float> zero (v.size(), 0.0f), inv (v.size());
        for (size_t i = 0; i < v.size(); ++i) inv[i] = -v[i];
        auto a = detect (v, &zero, sr, 0.6f);
        CHECK (hit (a, sr, v.size(), 0.5, 0.75), "stereo: S only in left channel");
        auto b = detect (v, &inv, sr, 0.6f);
        CHECK (hit (b, sr, v.size(), 0.5, 0.75), "stereo: out-of-phase channels (mid is empty)");
        auto c = detect (v, &v, sr, 0.6f);
        CHECK (hit (c, sr, v.size(), 0.5, 0.75), "stereo: identical channels");
    }
    // Detection -> render: sibilance emphasis changes the riser, stays finite and peak-safe
    {
        const double sr = 48000;
        auto v = phrase (sr);
        auto det = detect (v, nullptr, sr, 0.6f);
        const float* ch[1] = { v.data() };
        RenderSettings a, b; a.durationSec = b.durationSec = 3;
        b.sibAmount = 1.0f; b.sibRegions = det.regions;
        auto ra = renderReverseReverb (ch, 1, v.size(), sr, a), rb = renderReverseReverb (ch, 1, v.size(), sr, b);
        double diff = 0, hfA = 0, hfB = 0;
        for (size_t i = 1; i < ra.length(); ++i)
        {
            diff += std::fabs (ra.ch[0][i] - rb.ch[0][i]);
            const double da = ra.ch[0][i] - ra.ch[0][i - 1], db = rb.ch[0][i] - rb.ch[0][i - 1];
            hfA += da * da; hfB += db * db;
        }
        float pk = 0; for (float x : rb.ch[0]) pk = std::max (pk, std::fabs (x));
        std::printf ("      first-difference energy (brightness proxy): plain %.2f  sibilance %.2f\n", hfA, hfB);
        CHECK (rb.ok && rb.length() == ra.length() && diff > 1.0 && pk <= 0.989f + 1e-6f, "sibilance emphasis alters the render, peak-safe");
        CHECK (hfB / std::max (1e-9, hfA) > 1.0, "sibilance emphasis makes the riser brighter");
        RenderSettings c = b; c.sibRegions.clear();
        auto rc = renderReverseReverb (ch, 1, v.size(), sr, c);
        CHECK (rc.ch[0] == ra.ch[0], "no regions -> identical to plain render (whole source used)");
    }
}

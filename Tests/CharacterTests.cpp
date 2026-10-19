#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <string>
#include "Characters.h"
#include "SettingsFromParams.h"

extern int failures;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s  [%s]\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

using namespace jmc;

void runCharacterTests();
void runCharacterTests()
{
    const double sr = 48000;
    // A phrase-like source: decaying harmonic tone + a noise burst.
    std::vector<float> x ((size_t) (1.2 * sr));
    std::mt19937 g (3); std::uniform_real_distribution<float> d (-1, 1);
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / sr;
        double v = 0; for (int k = 1; k <= 12; ++k) v += std::sin (2 * 3.14159265 * 180.0 * k * t) / k;
        x[i] = (float) (0.2 * v * std::exp (-1.5 * t));
        if (t > 0.7 && t < 0.95) x[i] += 0.3f * d (g);
    }
    const float* ch[1] = { x.data() };

    std::vector<RenderResult> results;
    for (int c = 0; c < characters::count; ++c)
    {
        std::map<std::string, float> m;
        for (auto& [id, v] : characters::resolve (c)) m[id] = v;
        m["sync"] = 0; m["trimstart"] = 0; m["trimend"] = 1;
        auto s = settingsFromParams ([&] (const char* id) { return m.at (id); }, 120.0);
        s.sibRegions = { { 0.58f, 0.8f } }; // pretend detection found the noise burst
        auto r = renderReverseReverb (ch, 1, x.size(), sr, s);
        bool fin = true; float pk = 0;
        for (auto& c2 : r.ch) for (float v : c2) { if (! std::isfinite (v)) fin = false; pk = std::max (pk, std::fabs (v)); }
        char msg[128]; std::snprintf (msg, sizeof msg, "%-16s renders: finite, peak-safe, riser %.2f s, mono %.1f dB", characters::names[c], (double) r.riserSamples / sr, r.monoLossDb);
        CHECK (r.ok && fin && pk <= 0.989f + 1e-6f && r.riserSamples == (size_t) std::llround ((double) s.durationSec * sr) && r.monoLossDb > -6.3f, msg);
        results.push_back (std::move (r));
    }
    // Each mode must be a real, audibly different configuration - compare against every other.
    bool allDifferent = true;
    for (size_t a = 0; a < results.size(); ++a)
        for (size_t b = a + 1; b < results.size(); ++b)
        {
            const size_t n = std::min (results[a].length(), results[b].length());
            double diff = 0, ea = 0;
            for (size_t i = 0; i < n; ++i)
            {
                const double da = results[a].ch[0][i], db = results[b].ch[0][i];
                diff += (da - db) * (da - db); ea += da * da;
            }
            const bool lenDiff = results[a].length() != results[b].length();
            if (! lenDiff && diff < 0.05 * ea) { allDifferent = false; std::printf ("      modes %zu and %zu are too similar\n", a + 1, b + 1); }
        }
    CHECK (allDifferent, "all 12 character modes produce clearly different output");
    // Characters must not touch tempo sync or trim (they live outside the table)
    bool untouched = true;
    for (int c = 0; c < characters::count; ++c)
        for (auto& [id, v] : characters::resolve (c))
        {
            (void) v;
            const std::string n (id);
            if (n == "sync" || n == "trimstart" || n == "trimend") untouched = false;
        }
    CHECK (untouched, "characters never modify sync or trim");
    // Spot-check semantics
    auto cfg = [] (int c, const char* id) { for (auto& [k, v] : characters::resolve (c)) if (std::string (k) == id) return v; return -999.0f; };
    CHECK (cfg (6, "pitch") < 0 && cfg (7, "pitch") > 0, "PITCH RISE starts low, PITCH FALL starts high");
    CHECK (cfg (8, "gaterate") > 0 && cfg (10, "dry") > 0 && cfg (11, "sibon") > 0.5f && cfg (9, "width") > 1.5f, "GLITCH gates, IMPACT appends hit, SIB LIFT enables detection, WIDE widens");
    CHECK (cfg (5, "length") < cfg (0, "length") && cfg (4, "decay") > cfg (0, "decay") && cfg (3, "highcut") < 5000, "TIGHT shorter, HUGE longer decay, DARK filtered");
}

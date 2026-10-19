// Performance measurements: jmc_bench
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>
#include "dsp/RenderSettings.h"
#include "dsp/Sibilance.h"
#include "audio/Resampler.h"

using Clock = std::chrono::steady_clock;
static double since (Clock::time_point t) { return std::chrono::duration<double> (Clock::now() - t).count(); }

static std::vector<float> voiceish (double sr, double sec, unsigned seed)
{
    std::mt19937 g (seed); std::uniform_real_distribution<float> d (-1, 1);
    std::vector<float> v ((size_t) (sr * sec));
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = (double) i / sr;
        double s = 0; for (int k = 1; k <= 10; ++k) s += std::sin (6.2831853 * 170.0 * k * t) / k;
        v[i] = (float) (0.25 * s * (0.5 + 0.5 * std::sin (2.0 * t))) + ((int) (t * 2) % 5 == 0 ? 0.1f * d (g) : 0.0f);
    }
    return v;
}

int main()
{
    struct Case { const char* name; double sr, srcSec; float length, decay; };
    const Case cases[] = {
        { "10 s source, 4 s riser,  48 kHz", 48000, 10, 4, 2.5f },
        { "10 s source, 16 s riser, 48 kHz, decay 12", 48000, 10, 16, 12 },
        { "30 s source, 8 s riser, 96 kHz", 96000, 30, 8, 4 },
        { "10 s source, 30 s riser, 192 kHz, decay 20", 192000, 10, 30, 20 },
    };
    for (auto& c : cases)
    {
        auto x = voiceish (c.sr, c.srcSec, 1);
        const float* ch[2] = { x.data(), x.data() };
        jmc::RenderSettings s; s.durationSec = c.length; s.decaySec = c.decay;
        s.tiltDb = 2; s.modDepth = 0.4f; s.drive = 0.3f; s.dryLevel = 1; s.tailSec = 2;
        auto t = Clock::now();
        auto r = jmc::renderReverseReverb (ch, 2, x.size(), c.sr, s);
        std::printf ("render  %-48s %.3f s  (ok=%d, %.1f s of audio out)\n", c.name, since (t), r.ok, (double) r.length() / c.sr);
    }
    for (double sr : { 48000.0, 192000.0 })
    {
        auto x = voiceish (sr, 60, 2);
        const float* ch[2] = { x.data(), x.data() };
        auto t = Clock::now();
        auto f = jmc::analyseSibilance (ch, 2, x.size(), sr);
        std::printf ("sibilance analysis, 60 s stereo @ %6.0f Hz            %.3f s\n", sr, since (t));
        t = Clock::now();
        for (int i = 0; i < 100; ++i) jmc::detectSibilance (*f, 0.5f + 0.001f * (float) i);
        std::printf ("sibilance threshold stage (per call)                    %.3f ms\n", since (t) * 10.0);
    }
    {
        auto x = voiceish (44100, 60, 3);
        auto t = Clock::now();
        auto y = jmc::resampleSinc (x.data(), x.size(), 44100.0, 48000.0);
        std::printf ("resample 60 s mono 44.1k -> 48k                         %.3f s\n", since (t));
        (void) y;
    }
    return 0;
}

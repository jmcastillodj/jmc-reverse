#include "Sibilance.h"
#include "Fft.h"
#include <algorithm>
#include <cmath>

namespace jmc
{
std::shared_ptr<SibFeatures> analyseSibilance (const float* const* ch, int numCh, size_t n, double sr,
                                               const std::function<bool()>& cancel)
{
    auto f = std::make_shared<SibFeatures>();
    f->sampleRate = sr;
    f->numSamples = n;
    if (n == 0 || sr <= 0 || numCh < 1) return f;

    size_t N = 256;
    while ((double) N < 0.0213 * sr) N <<= 1;
    const size_t hop = N / 4;
    f->frameLen = N;
    f->hop = hop;
    const size_t frames = n > N ? (n - N) / hop + 1 : 1;
    f->ratio.assign (frames, 0.0f);
    f->centroid.assign (frames, 0.0f);
    f->flatness.assign (frames, 0.0f);
    f->levelDb.assign (frames, -200.0f);
    f->hiLoDb.assign (frames, -200.0f);

    constexpr double pi = 3.14159265358979323846;
    std::vector<float> win (N);
    double wsum = 0;
    for (size_t i = 0; i < N; ++i) { win[i] = (float) (0.5 - 0.5 * std::cos (2 * pi * (double) i / (double) N)); wsum += win[i]; }
    const double norm = 4.0 / (wsum * wsum); // unit-amplitude sine -> power 1

    const double binHz = sr / (double) N;
    auto bin = [&] (double hz) { return (size_t) std::clamp<double> (std::round (hz / binHz), 0.0, (double) N / 2); };
    const size_t nyq = N / 2;
    const size_t loA = bin (150), loB = bin (3000);
    const size_t hiA = bin (4500), hiB = std::min (bin (11000), (size_t) ((double) nyq * 0.95));
    const size_t totA = bin (80);

    Fft fft (N);
    std::vector<float> re (N), im (N);
    std::vector<double> P (nyq + 1);

    for (size_t fr = 0; fr < frames; ++fr)
    {
        if ((fr & 255) == 0 && cancel && cancel()) return nullptr;
        double bestLevel = -200.0;
        for (int c = 0; c < std::min(numCh, 2); ++c)
        {
            const size_t off = fr * hop;
            for (size_t i = 0; i < N; ++i)
            {
                re[i] = (off + i < n ? ch[c][off + i] : 0.0f) * win[i];
                im[i] = 0.0f;
            }
            fft.forward (re.data(), im.data());
            for (size_t k = 0; k <= nyq; ++k)
                P[k] = ((double) re[k] * re[k] + (double) im[k] * im[k]) * norm;

            double eHi = 0, eLo = 0, eTot = 0, cen = 0, logSum = 0;
            for (size_t k = totA; k <= nyq; ++k) { eTot += P[k]; cen += P[k] * (double) k * binHz; }
            for (size_t k = loA; k <= loB; ++k) eLo += P[k];
            for (size_t k = hiA; k <= hiB; ++k) { eHi += P[k]; logSum += std::log (P[k] + 1.0e-20); }
            const double cnt = (double) (hiB - hiA + 1);
            const double level = 10.0 * std::log10 (eHi + 1.0e-20);
            if (level > bestLevel)
            {
                bestLevel = level;
                f->levelDb[fr] = (float) level;
                f->ratio[fr] = (float) (eHi / (eTot + 1.0e-20));
                f->centroid[fr] = (float) (eTot > 1.0e-20 ? cen / eTot : 0.0);
                f->flatness[fr] = (float) (std::exp (logSum / cnt) / (eHi / cnt + 1.0e-20));
                f->hiLoDb[fr] = (float) (10.0 * std::log10 ((eHi + 1.0e-20) / (eLo + 1.0e-20)));
            }
        }
        f->maxLevelDb = std::max (f->maxLevelDb, f->levelDb[fr]);
    }
    return f;
}

SibResult detectSibilance (const SibFeatures& f, float sensitivity)
{
    SibResult r;
    const size_t frames = f.ratio.size();
    if (frames == 0 || f.hop == 0)
    {
        r.message = "No reliable sibilant region found. Using the entire source.";
        return r;
    }
    const float s = std::clamp (sensitivity, 0.0f, 1.0f);
    const float ratioT = 0.55f - 0.30f * s, centT = 5200.0f - 1400.0f * s, relDb = -(20.0f + 20.0f * s),
                flatT = 0.25f, hiLoT = 3.0f - 3.0f * s, absGate = -75.0f;

    std::vector<char> pass (frames, 0);
    for (size_t i = 0; i < frames; ++i)
        pass[i] = f.levelDb[i] > absGate && f.levelDb[i] >= f.maxLevelDb + relDb && f.ratio[i] >= ratioT
                  && f.centroid[i] >= centT && f.flatness[i] >= flatT && f.hiLoDb[i] >= hiLoT;

    const double hopSec = (double) f.hop / f.sampleRate;
    const size_t gap = (size_t) std::ceil (0.020 / hopSec);
    // Bridge short gaps
    for (size_t i = 0; i < frames;)
    {
        if (pass[i]) { ++i; continue; }
        size_t j = i;
        while (j < frames && ! pass[j]) ++j;
        if (i > 0 && j < frames && j - i <= gap)
            std::fill (pass.begin() + (long) i, pass.begin() + (long) j, 1);
        i = j;
    }

    // Sibilant-band energy that is present most of the time is not speech sibilance (noise, cymbal wash).
    const float passFraction = (float) std::count (pass.begin(), pass.end(), 1) / (float) frames;
    if (passFraction > 0.55f)
    {
        r.message = "High-frequency noise is present throughout - this does not look like a vocal. Using the entire source.";
        return r;
    }

    const double total = (double) f.numSamples;
    double covered = 0;
    for (size_t i = 0; i < frames;)
    {
        if (! pass[i]) { ++i; continue; }
        size_t j = i;
        while (j < frames && pass[j]) ++j;
        const double a = std::max (0.0, (double) (i * f.hop + f.frameLen / 2) - 0.5 * (double) f.hop);
        const double b = std::min (total, (double) ((j - 1) * f.hop + f.frameLen / 2) + 0.5 * (double) f.hop);
        const double dur = (b - a) / f.sampleRate;
        if (dur >= 0.040 && dur <= 0.600)
        {
            r.regions.push_back ({ (float) (a / total), (float) (b / total) });
            covered += b - a;
        }
        i = j;
    }
    r.coverage = (float) (covered / total);

    if (r.regions.empty())
    {
        r.message = "No reliable sibilant region found. Using the entire source.";
        return r;
    }
    r.reliable = true;
    r.message = std::to_string (r.regions.size()) + (r.regions.size() == 1 ? " sibilant region detected." : " sibilant regions detected.");
    return r;
}
} // namespace jmc

#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// Offline windowed-sinc sample-rate converter (Blackman-Harris, 16 zero crossings per side).
// Not real-time safe: allocates. Use on background threads / export only.
namespace jmc
{
inline std::vector<float> resampleSinc (const float* in, size_t n, double inRate, double outRate)
{
    if (n == 0 || inRate <= 0.0 || outRate <= 0.0)
        return {};
    if (std::abs (inRate - outRate) < 1.0e-6)
        return std::vector<float> (in, in + n);

    constexpr double pi = 3.14159265358979323846;
    constexpr int zeros = 16;
    constexpr int res = 256; // table entries per input sample

    const double scale = std::min (1.0, outRate / inRate) * 0.975;
    const double half  = zeros / scale;

    std::vector<double> table (size_t (half * res) + 3);
    for (size_t i = 0; i < table.size(); ++i)
    {
        const double x = double (i) / res;
        const double px = pi * scale * x;
        const double s = x < 1.0e-12 ? 1.0 : std::sin (px) / px;
        const double t = (x / half + 1.0) * 0.5; // 0..1 across the window
        double w = 0.0;
        if (t >= 0.0 && t <= 1.0)
            w = 0.35875 - 0.48829 * std::cos (2 * pi * t) + 0.14128 * std::cos (4 * pi * t)
                - 0.01168 * std::cos (6 * pi * t);
        table[i] = scale * s * w;
    }

    const size_t nOut = size_t (std::llround (double (n) * outRate / inRate));
    std::vector<float> out (nOut);
    const double ratio = inRate / outRate;

    for (size_t j = 0; j < nOut; ++j)
    {
        const double c = double (j) * ratio;
        const long long i0 = (long long) std::ceil (c - half);
        const long long i1 = (long long) std::floor (c + half);
        double acc = 0.0;
        for (long long i = std::max<long long> (i0, 0); i <= std::min<long long> (i1, (long long) n - 1); ++i)
        {
            const double idx = std::abs (double (i) - c) * res;
            const size_t k = size_t (idx);
            const double f = idx - double (k);
            if (k + 1 >= table.size())
                continue;
            acc += double (in[i]) * (table[k] * (1.0 - f) + table[k + 1] * f);
        }
        out[j] = float (acc);
    }
    return out;
}
} // namespace jmc

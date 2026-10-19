#pragma once
#include <cmath>
#include <utility>
#include <cstddef>
#include <vector>

namespace jmc
{
// Minimal in-place radix-2 complex FFT (analysis use; not real-time).
class Fft
{
public:
    explicit Fft (size_t n) : N (n), cosT (n / 2), sinT (n / 2), rev (n)
    {
        constexpr double pi = 3.14159265358979323846;
        for (size_t i = 0; i < n / 2; ++i)
        {
            cosT[i] = (float) std::cos (2 * pi * (double) i / (double) n);
            sinT[i] = (float) -std::sin (2 * pi * (double) i / (double) n);
        }
        size_t bits = 0;
        while ((size_t (1) << bits) < n) ++bits;
        for (size_t i = 0; i < n; ++i)
        {
            size_t r = 0;
            for (size_t b = 0; b < bits; ++b)
                if (i & (size_t (1) << b)) r |= size_t (1) << (bits - 1 - b);
            rev[i] = r;
        }
    }
    size_t size() const { return N; }

    void forward (float* re, float* im) const
    {
        for (size_t i = 0; i < N; ++i)
            if (rev[i] > i) { std::swap (re[i], re[rev[i]]); std::swap (im[i], im[rev[i]]); }
        for (size_t len = 2; len <= N; len <<= 1)
        {
            const size_t half = len / 2, step = N / len;
            for (size_t i = 0; i < N; i += len)
                for (size_t j = 0; j < half; ++j)
                {
                    const float wr = cosT[j * step], wi = sinT[j * step];
                    const size_t a = i + j, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr;        im[a] += ti;
                }
        }
    }

private:
    size_t N;
    std::vector<float> cosT, sinT;
    std::vector<size_t> rev;
};
} // namespace jmc

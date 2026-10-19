#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace jmc
{
// 8-line feedback delay network: orthogonal (Hadamard) feedback matrix, per-line RT60 gain,
// one-pole damping in the loop, slowly modulated delay taps. Wet signal only (no direct path).
// Offline use: allocates in prepare().
class FdnReverb
{
public:
    static constexpr int N = 8;
    void prepare (double sampleRate, float decaySec, float size, float damping, float predelaySec, float lowDamp = 0.0f);
    void process (const float* inL, const float* inR, float* outL, float* outR, size_t n);

private:
    struct Line { std::vector<float> buf; size_t mask = 0, w = 0; double len = 0; float g = 0, lp = 0, lf = 0; double phase = 0, inc = 0; };
    std::array<Line, N> lines;
    // Input diffusion: four series Schroeder all-passes per channel smear every input sample into
    // dozens of echoes, so the first 200 ms are dense (no audible flutter echoes after reversal).
    struct Allpass
    {
        std::vector<float> buf; size_t idx = 0; float g = 0.0f;
        void init (size_t len, float gain) { buf.assign (std::max<size_t> (1, len), 0.0f); idx = 0; g = gain; }
        float process (float x)
        {
            const float d = buf[idx];
            const float v = x - g * d;
            buf[idx] = v;
            if (++idx >= buf.size()) idx = 0;
            return d + g * v;
        }
    };
    std::array<Allpass, 4> diffL, diffR;
    std::vector<float> preL, preR;
    size_t preW = 0, preLen = 0;
    float damp = 0, lowK = 0, lfCoef = 0;
    double modDepth = 0;
};
} // namespace jmc

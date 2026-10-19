#pragma once
#include <cmath>
#include <vector>

namespace jmc
{
// RBJ 2nd-order Butterworth low/high-pass, direct form I (offline use).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    static Biquad make (bool highPass, double fc, double sr)
    {
        constexpr double pi = 3.14159265358979323846;
        fc = std::fmin (fc, sr * 0.45);
        const double w = 2 * pi * fc / sr, cw = std::cos (w), al = std::sin (w) / (2 * 0.70710678);
        const double a0 = 1 + al;
        Biquad q;
        if (highPass) { q.b0 = (1 + cw) / 2 / a0; q.b1 = -(1 + cw) / a0; q.b2 = q.b0; }
        else          { q.b0 = (1 - cw) / 2 / a0; q.b1 = (1 - cw) / a0;  q.b2 = q.b0; }
        q.a1 = -2 * cw / a0;
        q.a2 = (1 - al) / a0;
        return q;
    }
    // RBJ shelving filter (slope S = 1)
    static Biquad makeShelf (bool highShelf, double fc, double gainDb, double sr)
    {
        constexpr double pi = 3.14159265358979323846;
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * pi * std::fmin (fc, sr * 0.45) / sr;
        const double cw = std::cos (w), al = std::sin (w) / std::sqrt (2.0), t = 2 * std::sqrt (A) * al;
        double b0, b1, b2, a0, a1, a2;
        if (! highShelf)
        {
            b0 = A * ((A + 1) - (A - 1) * cw + t);  b1 = 2 * A * ((A - 1) - (A + 1) * cw);  b2 = A * ((A + 1) - (A - 1) * cw - t);
            a0 = (A + 1) + (A - 1) * cw + t;        a1 = -2 * ((A - 1) + (A + 1) * cw);     a2 = (A + 1) + (A - 1) * cw - t;
        }
        else
        {
            b0 = A * ((A + 1) + (A - 1) * cw + t);  b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - t);
            a0 = (A + 1) - (A - 1) * cw + t;        a1 = 2 * ((A - 1) - (A + 1) * cw);      a2 = (A + 1) - (A - 1) * cw - t;
        }
        Biquad q;
        q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0; q.a1 = a1 / a0; q.a2 = a2 / a0;
        return q;
    }
    void process (std::vector<float>& v)
    {
        for (auto& s : v)
        {
            const double x = s;
            const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            s = (float) y;
        }
    }
};
} // namespace jmc

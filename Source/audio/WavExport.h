#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

#if defined(__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wfloat-equal"
#endif
namespace jmc
{
struct ExportOptions
{
    int bits = 24;            // 24 (integer PCM) or 32 (IEEE float)
    double sampleRate = 0.0;  // 0 = keep the render's rate; else 44100/48000/88200/96000/192000
    bool mono = false;        // sum to one channel

    bool operator== (const ExportOptions&) const = default;
};

struct ExportResult
{
    bool ok = false;
    juce::String error;
    int frames = 0;
    double sampleRate = 0.0;
    int channels = 0;
    float peakScale = 1.0f;   // < 1 when peak protection had to scale the file (never a limiter)
};

// Writes `src` (1 or 2 channels at srcRate) to `dest`. Same engine buffer the preview plays, so the
// file matches what was heard: with default options at the render rate and 32-bit float it is
// sample-identical. Writes to "<name>.part", re-opens it to verify the header and length, then
// renames, so an interrupted export never leaves a truncated .wav.
ExportResult exportWav (const juce::File& dest, const juce::AudioBuffer<float>& src, double srcRate, const ExportOptions&);
} // namespace jmc
#if defined(__clang__)
 #pragma clang diagnostic pop
#endif

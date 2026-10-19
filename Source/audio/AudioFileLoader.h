#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <memory>

namespace jmc
{
struct LoadedAudio
{
    juce::AudioBuffer<float> buffer; // 1 or 2 channels at the file's native rate
    double sampleRate = 0.0;
    juce::String name;
    int numChannels() const { return buffer.getNumChannels(); }
    double durationSeconds() const { return sampleRate > 0 ? buffer.getNumSamples() / sampleRate : 0.0; }
};

struct LoadResult
{
    std::shared_ptr<LoadedAudio> audio;
    juce::String error; // empty on success
    bool cancelled = false;
    bool ok() const { return audio != nullptr; }
};

constexpr double maxSourceSeconds = 600.0;

bool hasSupportedExtension (const juce::File&);
// Reads WAV/AIFF (mono or stereo). Thread-safe, no GUI access. shouldCancel may be empty.
LoadResult loadAudioFile (const juce::File&, const std::function<bool()>& shouldCancel = {});
} // namespace jmc

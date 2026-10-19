#include "AudioFileLoader.h"

namespace jmc
{
bool hasSupportedExtension (const juce::File& f)
{
    static const juce::StringArray exts { ".wav", ".wave", ".aif", ".aiff", ".aifc", ".flac", ".mp3", ".m4a", ".aac", ".caf", ".ogg" };
    return exts.contains (f.getFileExtension().toLowerCase());
}

LoadResult loadAudioFile (const juce::File& file, const std::function<bool()>& shouldCancel)
{
    LoadResult r;
    if (! file.existsAsFile())
    {
        r.error = "File not found.";
        return r;
    }
    if (! hasSupportedExtension (file))
    {
        r.error = "Unsupported file type. Use WAV, AIFF, FLAC, MP3, M4A/AAC, CAF or OGG.";
        return r;
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats(); // WAV, AIFF, FLAC, OGG and (macOS) the system codecs: MP3, M4A/AAC, CAF

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        r.error = "Could not read the file (corrupt or unsupported encoding).";
        return r;
    }
    if (reader->numChannels < 1 || reader->numChannels > 2)
    {
        r.error = "Only mono and stereo files are supported (file has "
                  + juce::String ((int) reader->numChannels) + " channels).";
        return r;
    }
    if (reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
    {
        r.error = "The file contains no audio.";
        return r;
    }
    if (double (reader->lengthInSamples) / reader->sampleRate > maxSourceSeconds)
    {
        r.error = "File is too long (limit " + juce::String ((int) maxSourceSeconds) + " s).";
        return r;
    }

    auto audio = std::make_shared<LoadedAudio>();
    const int total = (int) reader->lengthInSamples;
    audio->buffer.setSize ((int) reader->numChannels, total);
    audio->sampleRate = reader->sampleRate;
    audio->name = file.getFileNameWithoutExtension();

    constexpr int chunk = 1 << 16;
    for (int pos = 0; pos < total; pos += chunk)
    {
        if (shouldCancel && shouldCancel())
        {
            r.cancelled = true;
            return r;
        }
        const int n = juce::jmin (chunk, total - pos);
        juce::AudioBuffer<float> tmp (audio->buffer.getArrayOfWritePointers(), audio->buffer.getNumChannels(), pos, n);
        if (! reader->read (&tmp, 0, n, pos, true, true))
        {
            r.error = "Error while reading audio data.";
            return r;
        }
    }

    for (int c = 0; c < audio->buffer.getNumChannels(); ++c)
    {
        auto* d = audio->buffer.getWritePointer (c);
        for (int i = 0; i < total; ++i)
            if (! std::isfinite (d[i]))
                d[i] = 0.0f;
    }

    r.audio = std::move (audio);
    return r;
}
} // namespace jmc

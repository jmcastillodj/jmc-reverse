#include "WavExport.h"
#include "Resampler.h"

namespace jmc
{
ExportResult exportWav (const juce::File& dest, const juce::AudioBuffer<float>& src, double srcRate, const ExportOptions& opt)
{
    ExportResult r;
    if (src.getNumSamples() <= 0 || src.getNumChannels() < 1 || srcRate <= 0.0)
    {
        r.error = "There is nothing to export.";
        return r;
    }
    if (opt.bits != 24 && opt.bits != 32)
    {
        r.error = "Unsupported bit depth.";
        return r;
    }

    const double outRate = opt.sampleRate > 0.0 ? opt.sampleRate : srcRate;
    const int srcCh = juce::jmin (2, src.getNumChannels());
    std::vector<std::vector<float>> ch ((size_t) srcCh);
    for (int c = 0; c < srcCh; ++c)
    {
        const float* d = src.getReadPointer (c);
        ch[(size_t) c] = std::abs (outRate - srcRate) < 1.0e-6 ? std::vector<float> (d, d + src.getNumSamples())
                                                                : resampleSinc (d, (size_t) src.getNumSamples(), srcRate, outRate);
    }
    if (opt.mono && srcCh == 2)
    {
        for (size_t i = 0; i < ch[0].size(); ++i)
            ch[0][i] = 0.5f * (ch[0][i] + ch[1][i]);
        ch.resize (1);
    }

    float peak = 0.0f;
    for (auto& c : ch)
        for (float v : c)
        {
            if (! std::isfinite (v)) { r.error = "The audio contains invalid samples."; return r; }
            peak = juce::jmax (peak, std::abs (v));
        }
    if (peak > 0.989f) // scale only: no limiter, no clipping
    {
        r.peakScale = 0.989f / peak;
        for (auto& c : ch) for (auto& v : c) v *= r.peakScale;
    }

    const int frames = (int) ch[0].size();
    juce::AudioBuffer<float> out ((int) ch.size(), frames);
    for (int c = 0; c < out.getNumChannels(); ++c)
        out.copyFrom (c, 0, ch[(size_t) c].data(), frames);

    if (! dest.getParentDirectory().createDirectory().wasOk())
    {
        r.error = "Could not create the destination folder.";
        return r;
    }
    const auto part = dest.getSiblingFile (dest.getFileName() + ".part");
    part.deleteFile();
    {
        std::unique_ptr<juce::OutputStream> os (part.createOutputStream());
        if (os == nullptr)
        {
            r.error = "Could not create the file (check the folder's permissions).";
            return r;
        }
        juce::WavAudioFormat wav;
        auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (outRate).withNumChannels (out.getNumChannels())
                                              .withBitsPerSample (opt.bits));
        if (w == nullptr || ! w->writeFromAudioSampleBuffer (out, 0, frames))
        {
            part.deleteFile();
            r.error = "Writing failed (disk full?).";
            return r;
        }
    } // writer closes here, flushing the header

    // Verify what actually reached the disk.
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> rd (wav.createReaderFor (part.createInputStream().release(), true));
        if (rd == nullptr || rd->lengthInSamples != frames || (int) rd->numChannels != out.getNumChannels()
            || std::abs (rd->sampleRate - outRate) > 0.5)
        {
            part.deleteFile();
            r.error = "The exported file failed verification.";
            return r;
        }
    }
    dest.deleteFile();
    if (! part.moveFileTo (dest))
    {
        part.deleteFile();
        r.error = "Could not save to the chosen location.";
        return r;
    }
    r.ok = true;
    r.frames = frames;
    r.sampleRate = outRate;
    r.channels = out.getNumChannels();
    return r;
}
} // namespace jmc

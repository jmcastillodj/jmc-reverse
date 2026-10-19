#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <cstdio>
#include "audio/AudioFileLoader.h"
#include "audio/Resampler.h"

int failures = 0;
void runEngineTests();
void runSibilanceTests();
void runEffectsTests();
void runCharacterTests();
void runEffectsTests2();
void runExportTests();
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s  [%s]\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

static std::vector<float> sine (double f, double sr, size_t n, float amp = 0.5f)
{
    std::vector<float> v (n);
    for (size_t i = 0; i < n; ++i)
        v[i] = amp * (float) std::sin (2.0 * 3.14159265358979323846 * f * (double) i / sr);
    return v;
}

static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0;
    for (size_t i = a; i < b; ++i) s += double (v[i]) * v[i];
    return std::sqrt (s / double (b - a));
}

static juce::File writeFile (const juce::File& f, juce::AudioFormat& fmt, int ch, double sr, int bits, int n)
{
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (ch).withBitsPerSample (bits);
    std::unique_ptr<juce::AudioFormatWriter> w = fmt.createWriterFor (os, opts);
    juce::AudioBuffer<float> b (ch, n);
    for (int c = 0; c < ch; ++c)
    {
        auto s = sine (440.0 * (c + 1), sr, (size_t) n);
        std::copy (s.begin(), s.end(), b.getWritePointer (c));
    }
    w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

int main()
{
    // --- Resampler ---
    {
        auto in = sine (1000.0, 48000.0, 48000);
        auto out = jmc::resampleSinc (in.data(), in.size(), 48000.0, 44100.0);
        CHECK (out.size() == 44100, "48k->44.1k length");
        CHECK (std::abs (rms (out, 2000, 42000) - 0.5 / std::sqrt (2.0)) < 0.005, "48k->44.1k tone level preserved");
    }
    {
        auto in = sine (30000.0, 96000.0, 96000);
        auto out = jmc::resampleSinc (in.data(), in.size(), 96000.0, 48000.0);
        CHECK (rms (out, 2000, 46000) < 0.0015, "96k->48k rejects 30 kHz alias (>50 dB)");
    }
    {
        auto in = sine (3000.0, 44100.0, 44100);
        auto up = jmc::resampleSinc (in.data(), in.size(), 44100.0, 192000.0);
        auto back = jmc::resampleSinc (up.data(), up.size(), 192000.0, 44100.0);
        CHECK (back.size() == in.size(), "44.1k->192k->44.1k length");
        double e = 0;
        for (size_t i = 4000; i < 40000; ++i) e += double (back[i] - in[i]) * (back[i] - in[i]);
        e = std::sqrt (e / 36000.0);
        CHECK (e < 0.002, "round trip error < -50 dB");
    }
    {
        std::vector<float> one (1, 1.0f);
        auto o = jmc::resampleSinc (one.data(), 1, 44100.0, 48000.0);
        CHECK (o.size() == 1, "1-sample source does not crash");
        auto e = jmc::resampleSinc (one.data(), 0, 44100.0, 48000.0);
        CHECK (e.empty(), "empty source");
    }

    runEngineTests();
    runSibilanceTests();
    runEffectsTests();
    runCharacterTests();
    runEffectsTests2();
    runExportTests();

    // --- Loader ---
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("jmc_tests");
    dir.createDirectory();
    {
        juce::WavAudioFormat wav;
        auto f = writeFile (dir.getChildFile ("st24.wav"), wav, 2, 96000.0, 24, 9600);
        auto r = jmc::loadAudioFile (f);
        CHECK (r.ok() && r.audio->numChannels() == 2 && r.audio->buffer.getNumSamples() == 9600
                   && r.audio->sampleRate == 96000.0, "24-bit stereo WAV 96k loads");
        auto f2 = writeFile (dir.getChildFile ("m16.wav"), wav, 1, 44100.0, 16, 100);
        auto r2 = jmc::loadAudioFile (f2);
        CHECK (r2.ok() && r2.audio->numChannels() == 1, "16-bit mono WAV loads");
        auto f3 = writeFile (dir.getChildFile ("one.wav"), wav, 1, 44100.0, 16, 1);
        CHECK (jmc::loadAudioFile (f3).ok(), "1-sample WAV loads");
        auto f4 = writeFile (dir.getChildFile ("flt.wav"), wav, 2, 192000.0, 32, 1000);
        CHECK (jmc::loadAudioFile (f4).ok(), "32-bit stereo WAV 192k loads");
    }
    {
        juce::AiffAudioFormat aif;
        auto f = writeFile (dir.getChildFile ("st.aif"), aif, 2, 48000.0, 24, 4800);
        auto r = jmc::loadAudioFile (f);
        CHECK (r.ok() && r.audio->numChannels() == 2, "24-bit stereo AIFF loads");
    }
    {
        juce::FlacAudioFormat flac;
        auto f = writeFile (dir.getChildFile ("st.flac"), flac, 2, 44100.0, 24, 4410);
        auto r = jmc::loadAudioFile (f);
        CHECK (r.ok() && r.audio->numChannels() == 2 && r.audio->buffer.getNumSamples() == 4410 && r.audio->sampleRate == 44100.0, "24-bit stereo FLAC loads (dropped clips may be any common format)");
        bool ext = true;
        for (auto* e : { "a.wav", "a.WAV", "a.aif", "a.aiff", "a.flac", "a.mp3", "a.m4a", "a.aac", "a.caf", "a.ogg" }) ext = ext && jmc::hasSupportedExtension (juce::File (e));
        CHECK (ext && ! jmc::hasSupportedExtension (juce::File ("a.txt")) && ! jmc::hasSupportedExtension (juce::File ("a")), "drop filter: audio extensions accepted, others ignored");
    }
    CHECK (! jmc::loadAudioFile (dir.getChildFile ("missing.wav")).ok(), "missing file -> error");
    {
        auto bad = dir.getChildFile ("bad.wav");
        bad.replaceWithText ("this is not a wav file");
        auto r = jmc::loadAudioFile (bad);
        CHECK (! r.ok() && r.error.isNotEmpty(), "corrupt WAV -> error, no crash");
        auto txt = dir.getChildFile ("x.mp3");
        txt.replaceWithText ("x");
        CHECK (! jmc::loadAudioFile (txt).ok(), "unsupported extension -> error");
    }
    {
        juce::WavAudioFormat wav;
        auto f = writeFile (dir.getChildFile ("c.wav"), wav, 2, 44100.0, 16, 100000);
        auto r = jmc::loadAudioFile (f, [] { return true; });
        CHECK (r.cancelled && ! r.ok(), "cancellation honoured");
    }
    dir.deleteRecursively();

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}

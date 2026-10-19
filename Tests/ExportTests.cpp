#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <cstdio>
#include "audio/WavExport.h"
#include "ExportNaming.h"

extern int failures;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s  [%s]\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

using namespace jmc;

namespace
{
juce::AudioBuffer<float> makeTone (int n, double sr, float amp, int ch = 2)
{
    juce::AudioBuffer<float> b (ch, n);
    for (int c = 0; c < ch; ++c)
        for (int i = 0; i < n; ++i)
            b.setSample (c, i, amp * (float) std::sin (2 * 3.14159265358979 * (440.0 * (c + 1)) * i / sr));
    return b;
}
std::unique_ptr<juce::AudioFormatReader> openWav (const juce::File& f)
{
    juce::WavAudioFormat wav;
    return std::unique_ptr<juce::AudioFormatReader> (wav.createReaderFor (f.createInputStream().release(), true));
}
}

void runExportTests();
void runExportTests()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("jmc_export_tests");
    dir.deleteRecursively();
    dir.createDirectory();
    const double sr = 48000;

    // Bit-exact 32-bit float at the render rate: the file IS the preview buffer
    {
        auto src = makeTone (48000, sr, 0.7f);
        auto f = dir.getChildFile ("f32.wav");
        auto r = exportWav (f, src, sr, { 32, 0.0, false });
        auto rd = openWav (f);
        bool same = r.ok && rd != nullptr && rd->lengthInSamples == 48000 && rd->numChannels == 2 && (int) rd->sampleRate == 48000;
        if (same)
        {
            juce::AudioBuffer<float> back (2, 48000);
            rd->read (&back, 0, 48000, 0, true, true);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 48000; ++i) if (std::abs (back.getSample (c, i) - src.getSample (c, i)) > 0.0f) same = false;
        }
        CHECK (same, "32-bit float export is sample-identical to the preview buffer");
        CHECK (! f.getSiblingFile ("f32.wav.part").exists(), "no .part file left behind");
    }
    // 24-bit: within half an LSB
    {
        auto src = makeTone (24000, sr, 0.5f);
        auto f = dir.getChildFile ("i24.wav");
        auto r = exportWav (f, src, sr, { 24, 0.0, false });
        auto rd = openWav (f);
        double err = 1;
        if (r.ok && rd != nullptr)
        {
            juce::AudioBuffer<float> back (2, 24000);
            rd->read (&back, 0, 24000, 0, true, true);
            err = 0;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 24000; ++i) err = std::max (err, (double) std::abs (back.getSample (c, i) - src.getSample (c, i)));
        }
        std::printf ("      24-bit max error %.3g (1 LSB = %.3g)\n", err, 1.0 / 8388608.0);
        CHECK (rd != nullptr && rd->bitsPerSample == 24 && err < 1.0 / 8388608.0, "24-bit export accurate to 1 LSB");
    }
    // Mono
    {
        auto src = makeTone (4800, sr, 0.4f);
        auto f = dir.getChildFile ("mono.wav");
        auto r = exportWav (f, src, sr, { 24, 0.0, true });
        auto rd = openWav (f);
        CHECK (r.ok && rd != nullptr && rd->numChannels == 1 && rd->lengthInSamples == 4800, "mono export writes one channel");
        juce::AudioBuffer<float> one (1, 4800), a (1, 4800);
        rd->read (&one, 0, 4800, 0, true, true);
        double e = 0; for (int i = 0; i < 4800; ++i) e = std::max (e, (double) std::abs (one.getSample (0, i) - 0.5f * (src.getSample (0, i) + src.getSample (1, i))));
        CHECK (e < 2e-7, "mono = (L+R)/2");
        juce::AudioBuffer<float> m1 (1, 2000);
        for (int i = 0; i < 2000; ++i) m1.setSample (0, i, 0.3f);
        auto r2 = exportWav (dir.getChildFile ("mono_in.wav"), m1, sr, { 24, 0.0, false });
        auto rd2 = openWav (dir.getChildFile ("mono_in.wav"));
        CHECK (r2.ok && rd2->numChannels == 1, "mono source stays mono");
    }
    // Sample-rate conversion
    for (double outRate : { 44100.0, 88200.0, 96000.0, 192000.0 })
    {
        auto src = makeTone (48000, sr, 0.5f);
        auto f = dir.getChildFile ("rate.wav");
        auto r = exportWav (f, src, sr, { 32, outRate, false });
        auto rd = openWav (f);
        const long long expect = std::llround (48000.0 * outRate / sr);
        double freqErr = 1e9;
        if (r.ok && rd != nullptr)
        {
            juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples);
            rd->read (&b, 0, (int) rd->lengthInSamples, 0, true, true);
            size_t zc = 0; const int a = (int) (rd->lengthInSamples / 4), e = (int) (rd->lengthInSamples * 3 / 4);
            for (int i = a + 1; i < e; ++i) if (b.getSample (0, i - 1) < 0 && b.getSample (0, i) >= 0) ++zc;
            freqErr = std::abs ((double) zc / ((double) (e - a) / outRate) - 440.0);
        }
        char msg[96]; std::snprintf (msg, sizeof msg, "export @ %.0f Hz: header, length (%lld) and pitch correct", outRate, expect);
        CHECK (rd != nullptr && (int) rd->sampleRate == (int) outRate && rd->lengthInSamples == expect && freqErr < 6.0, msg);
    }
    // Peak protection: scale only
    {
        auto src = makeTone (4800, sr, 1.6f);
        auto f = dir.getChildFile ("hot.wav");
        auto r = exportWav (f, src, sr, { 32, 0.0, false });
        auto rd = openWav (f);
        juce::AudioBuffer<float> b (2, 4800);
        rd->read (&b, 0, 4800, 0, true, true);
        const float pk = std::max (b.getMagnitude (0, 0, 4800), b.getMagnitude (1, 0, 4800));
        // shape preserved: ratio of two samples unchanged
        const float ratioIn = src.getSample (0, 37) / src.getSample (0, 91), ratioOut = b.getSample (0, 37) / b.getSample (0, 91);
        CHECK (r.ok && std::abs (pk - 0.989f) < 1e-4f && std::abs (ratioIn - ratioOut) < 1e-4f && r.peakScale < 1.0f, "over-range audio is scaled (no clipping, no limiting)");
        auto quiet = makeTone (4800, sr, 0.001f);
        exportWav (dir.getChildFile ("quiet.wav"), quiet, sr, { 32, 0.0, false });
        auto rq = openWav (dir.getChildFile ("quiet.wav"));
        juce::AudioBuffer<float> bq (2, 4800); rq->read (&bq, 0, 4800, 0, true, true);
        CHECK (std::abs (bq.getMagnitude (0, 0, 4800) - 0.001f) < 1e-6f, "quiet audio is never boosted");
    }
    // Overwrite and failure handling
    {
        auto src = makeTone (1000, sr, 0.3f);
        auto f = dir.getChildFile ("ow.wav");
        exportWav (f, src, sr, { 24, 0.0, false });
        auto src2 = makeTone (2000, sr, 0.3f);
        auto r = exportWav (f, src2, sr, { 24, 0.0, false });
        CHECK (r.ok && openWav (f)->lengthInSamples == 2000, "exporting over an existing file replaces it");
        // A destination whose parent "folder" is really a file can never be created, on any platform.
        auto blocker = dir.getChildFile ("blocker.txt");
        blocker.replaceWithText ("x");
        auto bad = exportWav (blocker.getChildFile ("x.wav"), src, sr, { 24, 0.0, false });
        CHECK (! bad.ok && bad.error.isNotEmpty(), "unwritable destination -> clear error, no crash");
        juce::AudioBuffer<float> empty;
        CHECK (! exportWav (dir.getChildFile ("e.wav"), empty, sr, { 24, 0.0, false }).ok, "empty buffer rejected");
        auto nan = makeTone (100, sr, 0.3f); nan.setSample (0, 5, std::nanf (""));
        CHECK (! exportWav (dir.getChildFile ("n.wav"), nan, sr, { 24, 0.0, false }).ok, "NaN audio rejected");
        CHECK (! exportWav (dir.getChildFile ("b.wav"), src, sr, { 16, 0.0, false }).ok, "unsupported bit depth rejected");
    }
    // Naming
    {
        CHECK (makeExportName ("Classic Reverse", 2.0, 120.0, 4, 4) == "JMCReverse_ClassicReverse_1bars_120bpm.wav", "name: 2 s @120 BPM 4/4 = 1 bar");
        CHECK (makeExportName ("PITCH RISE", 8.0, 120.0, 4, 4) == "JMCReverse_PitchRise_4bars_120bpm.wav", "name: 4 bars");
        CHECK (makeExportName ("Big Chorus Entry", 1.2, 100.0, 4, 4) == "JMCReverse_BigChorusEntry_0.5bars_100bpm.wav", "name: fractional bars");
        CHECK (makeExportName ("", 3.0, 98.5, 3, 4) == "JMCReverse_Custom_1.64bars_98.5bpm.wav", "name: custom effect, 3/4, fractional BPM");
        CHECK (makeExportName ("Ad-lib Throw / v2", 1.0, 120.0, 6, 8).startsWith ("JMCReverse_AdLibThroWV2") == false
                   && makeExportName ("Ad-lib Throw / v2", 1.0, 120.0, 6, 8).startsWith ("JMCReverse_AdLibThrowV2_"), "name: punctuation removed");
    }
    dir.deleteRecursively();
}

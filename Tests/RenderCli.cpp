// Offline renderer for auditioning: jmc_render in.wav out.wav [length_s] [decay_s] [dry 0..1] [char=1..12] [bpm=120] [mode=0 smooth tail | 1 phrase]
// char=N applies one of the 12 character modes (same table as the plugin buttons).
// Uses exactly the same engine as the plugin (no resampling: renders at the file's rate).
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <cstdlib>
#include "audio/AudioFileLoader.h"
#include "dsp/RenderSettings.h"
#include "Characters.h"
#include "SettingsFromParams.h"
#include <map>
#include <string>

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: jmc_render in.wav out.wav [length_s] [decay_s] [dry]\n"); return 2; }
    auto res = jmc::loadAudioFile (juce::File (argv[1]));
    if (! res.ok()) { std::printf ("load error: %s\n", res.error.toRawUTF8()); return 1; }

    int character = 0, mode = -1;
    double bpm = 120.0;
    std::vector<std::string> plain;
    for (int i = 3; i < argc; ++i)
    {
        std::string a (argv[i]);
        if (a.rfind ("char=", 0) == 0) character = std::atoi (a.c_str() + 5);
        else if (a.rfind ("bpm=", 0) == 0) bpm = std::atof (a.c_str() + 4);
        else if (a.rfind ("mode=", 0) == 0) mode = std::atoi (a.c_str() + 5);
        else plain.push_back (a);
    }
    jmc::RenderSettings s;
    if (character >= 1 && character <= jmc::characters::count)
    {
        std::map<std::string, float> m;
        for (auto& [id, v] : jmc::characters::resolve (character - 1)) m[id] = v;
        m["sync"] = 0; m["trimstart"] = 0; m["trimend"] = 1;
        s = jmc::settingsFromParams ([&] (const char* id) { return m.at (id); }, bpm);
        std::printf ("character: %s\n", jmc::characters::names[character - 1]);
    }
    if (mode >= 0) s.mode = mode;
    if (plain.size() > 0) s.durationSec = (float) std::atof (plain[0].c_str());
    if (plain.size() > 1) s.decaySec = (float) std::atof (plain[1].c_str());
    if (plain.size() > 2) s.dryLevel = (float) std::atof (plain[2].c_str());

    auto& b = res.audio->buffer;
    const float* ch[2] = { b.getReadPointer (0), b.getReadPointer (b.getNumChannels() > 1 ? 1 : 0) };
    auto r = jmc::renderReverseReverb (ch, b.getNumChannels(), (size_t) b.getNumSamples(), res.audio->sampleRate, s);
    if (! r.ok) { std::printf ("render failed\n"); return 1; }

    juce::AudioBuffer<float> out (2, (int) r.length());
    for (int c = 0; c < 2; ++c) std::copy (r.ch[c].begin(), r.ch[c].end(), out.getWritePointer (c));
    juce::File outFile (argv[2]);
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (outFile.createOutputStream());
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (res.audio->sampleRate).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) { std::printf ("cannot write output\n"); return 1; }
    w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    std::printf ("wrote %s (%.2f s, riser %.2f s)\n", argv[2], (double) r.length() / res.audio->sampleRate, (double) r.riserSamples / res.audio->sampleRate);
    return 0;
}

// Tests that instantiate the real processor: parameter IDs, state recall, undo, presets, async render.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>
#include "PluginProcessor.h"
#include "Presets.h"
#include "FactoryPresets.h"
#include "Characters.h"
#include "ParameterIDs.h"
#include "SettingsFromParams.h"
#include <map>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s  [%s]\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

static juce::File makeWav (const juce::File& f, double sr, int secondsTenths)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    const int n = (int) (sr * secondsTenths / 10.0);
    juce::AudioBuffer<float> b (2, n);
    juce::Random rnd (7);
    for (int i = 0; i < n; ++i)
    {
        const float env = std::exp (-3.0f * (float) i / (float) n);
        b.setSample (0, i, 0.4f * env * (rnd.nextFloat() * 2 - 1));
        b.setSample (1, i, 0.4f * env * std::sin (0.05f * (float) i));
    }
    w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

static bool waitFor (JMCReverseProcessor& p, jmc::EngineState want, int ms = 20000)
{
    for (int t = 0; t < ms / 10; ++t)
    {
        p.updateRenderIfNeeded();
        if (p.getState() == want) return true;
        if (p.getState() == jmc::EngineState::Error && want != jmc::EngineState::Error) return false;
        juce::Thread::sleep (10);
    }
    return false;
}

static float real (JMCReverseProcessor& p, const char* id)
{
    auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
    return r->convertFrom0to1 (r->getValue());
}
static void setReal (JMCReverseProcessor& p, const char* id, float v)
{
    auto* r = p.apvts.getParameter (id);
    r->setValueNotifyingHost (r->convertTo0to1 (v));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("jmc_plugin_tests");
    tmp.deleteRecursively();
    tmp.createDirectory();

    // ---- Parameter IDs are a public contract: hosts store them. Never change these.
    {
        JMCReverseProcessor p;
        const juce::StringArray golden { "mode", "length", "decay", "size", "predelay", "damping", "lowcut", "highcut", "width", "dry", "sync",
            "division", "preroll", "tail", "lowdamp", "tilt", "balance", "curve", "pitch", "moddepth", "modrate", "gaterate",
            "gatedepth", "gateramp", "drive", "chorus", "rampon", "rampmix", "rampa", "rampb", "rampc", "tremolo", "tremmix", "tremrate", "sibon", "sibsens", "sibamt", "fadein", "fadeout", "outgain", "trimstart", "trimend", "autolevel" };
        juce::StringArray have;
        for (auto* par : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (par)) have.add (r->getParameterID());
        bool same = have.size() == golden.size();
        for (auto& g : golden) if (! have.contains (g)) same = false;
        CHECK (same, "parameter IDs match the frozen list (43 parameters)");
        bool v1 = true;
        for (auto* par : p.getParameters())
            if (auto* wv = dynamic_cast<juce::AudioProcessorParameterWithID*> (par)) if (wv->paramID.isEmpty()) v1 = false;
        CHECK (v1, "every parameter has a non-empty ID");
    }

    // ---- Load, render, state round trip
    auto wav = makeWav (tmp.getChildFile ("src.wav"), 48000.0, 8);
    juce::MemoryBlock saved;
    {
        JMCReverseProcessor p;
        p.prepareToPlay (48000.0, 512);
        p.loadFile (wav);
        CHECK (waitFor (p, jmc::EngineState::Ready), "load -> analyse -> render reaches Ready");
        CHECK (p.getRendered() != nullptr && p.getRendered()->buffer.getNumSamples() > 0, "rendered audio is available");

        p.applyCharacter (9);
        setReal (p, "trimstart", 0.1f);
        setReal (p, "trimend", 0.8f);
        setReal (p, "sync", 1.0f);
        setReal (p, "division", 5.0f);
        setReal (p, "sibon", 1.0f);
        p.addSibRegion (0.2f, 0.3f);
        CHECK (waitFor (p, jmc::EngineState::Ready), "re-render after edits");
        p.getStateInformation (saved);
        CHECK (saved.getSize() > 100, "state saved");
    }
    {
        JMCReverseProcessor q;
        q.prepareToPlay (48000.0, 512);
        q.setStateInformation (saved.getData(), (int) saved.getSize());
        CHECK (waitFor (q, jmc::EngineState::Ready), "restored project reloads its source and renders");
        CHECK (q.getSourceFile() == wav, "source path restored");
        CHECK (q.getCharacter() == 9 && std::abs (real (q, "gaterate") - 2.0f) < 1e-3f, "character + parameters restored");
        CHECK (std::abs (real (q, "trimstart") - 0.1f) < 1e-3f && std::abs (real (q, "trimend") - 0.8f) < 1e-3f, "trim survives reload (not reset by file load)");
        CHECK (std::abs (real (q, "division") - 5.0f) < 1e-3f && real (q, "sync") > 0.5f, "tempo sync + division restored");
        bool hasEdit = false;
        for (auto& g : q.getSibRegions()) if (g.start <= 0.21f && g.end >= 0.29f) hasEdit = true;
        CHECK (hasEdit, "manual sibilance region restored");
        CHECK (q.getRendered() != nullptr, "restored project has a rendered result");

        // Corrupt / foreign state must be ignored without crashing
        const char junk[] = "not xml at all";
        q.setStateInformation (junk, (int) sizeof junk);
        CHECK (real (q, "gaterate") > 1.5f, "garbage state ignored");

        // Undo / redo (parameter edits, presets and characters are single steps)
        q.clearUndoHistory();
        const float before = real (q, "decay");
        setReal (q, "decay", 7.0f);
        q.undo();
        std::printf ("      decay before %.2f, after undo %.2f\n", before, real (q, "decay"));
        CHECK (std::abs (real (q, "decay") - before) < 1e-3f, "undo restores a parameter change");
        q.redo();
        CHECK (std::abs (real (q, "decay") - 7.0f) < 1e-2f, "redo re-applies it");
        q.clearUndoHistory();
        const int chBefore = q.getCharacter();
        q.applyCharacter (4);
        q.undo();
        CHECK (q.getCharacter() == chBefore && std::abs (real (q, "highcut") - 20000.0f) < 50.0f, "undo reverts a whole character change in one step");
        CHECK (! q.canUndo() && q.canRedo(), "undo/redo availability tracked");
    }
    // Missing source file
    {
        JMCReverseProcessor p;
        JMCReverseProcessor src;
        juce::MemoryBlock blk;
        src.loadFile (wav);
        waitFor (src, jmc::EngineState::Ready);
        src.getStateInformation (blk);
        wav.moveFileTo (tmp.getChildFile ("moved.wav"));
        p.setStateInformation (blk.getData(), (int) blk.getSize());
        CHECK (p.getState() == jmc::EngineState::Error && p.getErrorMessage().contains ("not found"), "missing source -> clear message, no crash");
        JMCReverseProcessor again;
        again.setStateInformation (blk.getData(), (int) blk.getSize());
        juce::MemoryBlock blk2;
        again.getStateInformation (blk2);
        CHECK (blk2.getSize() > 0, "state remains savable while the source is missing");
        tmp.getChildFile ("moved.wav").moveFileTo (wav);
    }

    // ---- Presets
    {
        JMCReverseProcessor p;
        p.prepareToPlay (48000.0, 512);
        jmc::PresetManager pm (p, tmp.getChildFile ("userpresets"));
        auto list = pm.list();
        const auto& fac = jmc::factory::all();
        CHECK (list.size() >= 24 && fac.size() >= 24, "at least 24 factory presets");
        juce::StringArray names;
        bool unique = true, cats = true;
        for (auto& pi : list)
        {
            if (names.contains (pi.name)) unique = false;
            names.add (pi.name);
            if (! (pi.category == "Vocals" || pi.category == "Ad-libs" || pi.category == "Transitions" || pi.category == "Sound Design")) cats = false;
        }
        CHECK (unique, "preset names are unique");
        CHECK (cats, "factory presets are categorised (Vocals / Ad-libs / Transitions / Sound Design)");

        p.loadFile (wav);
        waitFor (p, jmc::EngineState::Ready);
        bool allOk = true;
        for (auto& pi : list)
        {
            if (! pm.apply (pi)) { allOk = false; std::printf ("      cannot apply %s\n", pi.name.toRawUTF8()); continue; }
            for (auto& [id, v] : jmc::factory::resolve (fac[(size_t) pi.factoryIndex]))
            {
                auto* r = p.apvts.getParameter (id);
                if (r == nullptr) { allOk = false; std::printf ("      unknown param %s in %s\n", id, pi.name.toRawUTF8()); continue; }
                const float got = dynamic_cast<juce::RangedAudioParameter*> (r)->convertFrom0to1 (r->getValue());
                if (std::abs (got - v) > 0.02f * std::max (1.0f, std::abs (v))) { allOk = false; std::printf ("      %s.%s: wanted %.3f got %.3f (out of range?)\n", pi.name.toRawUTF8(), id, v, got); }
            }
        }
        CHECK (allOk, "every factory preset applies exactly (all IDs valid, values in range)");
        CHECK (waitFor (p, jmc::EngineState::Ready), "rendering settles after cycling all presets");

        // Latest settings win: the rendered riser must match the final length, not an earlier request.
        setReal (p, "sync", 0.0f);
        setReal (p, "length", 1.0f); p.requestRender();
        setReal (p, "length", 2.0f); p.requestRender();
        setReal (p, "length", 3.0f); p.requestRender();
        CHECK (waitFor (p, jmc::EngineState::Ready), "burst of render requests settles");
        for (int i = 0; i < 3000 && ! p.isResultCurrent(); ++i) { p.updateRenderIfNeeded(); juce::Thread::sleep (10); }
        waitFor (p, jmc::EngineState::Ready);
        auto r = p.getRendered();
        const double riser = p.getRiserSeconds();
        std::printf ("      onset after request burst: %.3f s (expected 3.000)\n", riser);
        CHECK (r != nullptr && std::abs (riser - 3.0) < 0.01, "stale renders never overwrite newer settings");

        // User presets: save / list / load / delete
        setReal (p, "decay", 6.5f);
        setReal (p, "tilt", -3.0f);
        setReal (p, "trimstart", 0.25f);
        juce::String err;
        CHECK (pm.saveUser ("My: Test/Preset", "Vocals", err), "user preset saves (illegal characters sanitised)");
        auto after = pm.list();
        jmc::PresetInfo mine;
        for (auto& pi : after) if (! pi.factory) mine = pi;
        CHECK (! mine.factory && mine.category == "Vocals", "user preset listed with its category");
        const auto xmlText = mine.file.loadFileAsString();
        CHECK (! xmlText.contains ("trimstart") && ! xmlText.contains (".wav") && ! xmlText.contains ("/Users"), "preset holds no trim, file name or absolute path");
        setReal (p, "decay", 1.0f); setReal (p, "tilt", 0.0f);
        CHECK (pm.apply (mine) && std::abs (real (p, "decay") - 6.5f) < 0.01f && std::abs (real (p, "tilt") + 3.0f) < 0.01f, "user preset loads back its values");
        CHECK (std::abs (real (p, "trimstart") - 0.25f) < 1e-3f, "loading a preset keeps the current trim");
        CHECK (! pm.saveUser ("   ", "User", err) && err.isNotEmpty(), "empty preset name rejected with message");
        CHECK (pm.deleteUser (mine) && pm.list().size() == list.size(), "user preset deletes");
    }


    // ---- Capture from the track input, auto-capture, loop, reset, FX toggles
    {
        struct TestPlayHead : juce::AudioPlayHead
        {
            bool playing = false;
            juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setIsPlaying (playing); i.setBpm (120.0); return i; }
        };
        auto feed = [] (JMCReverseProcessor& p, int blocks, float amp, bool sine = true, float* checkOutErr = nullptr)
        {
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            double ph = 0;
            for (int b = 0; b < blocks; ++b)
            {
                for (int s2 = 0; s2 < 256; ++s2, ph += 0.05)
                {
                    const float v = sine ? amp * (float) std::sin (ph) : 0.0f;
                    buf.setSample (0, s2, v); buf.setSample (1, s2, -v);
                }
                auto copy = buf;
                p.processBlock (buf, midi);
                if (checkOutErr != nullptr)
                    for (int c = 0; c < 2; ++c) for (int s2 = 0; s2 < 256; ++s2)
                        *checkOutErr = std::max (*checkOutErr, std::abs (buf.getSample (c, s2) - copy.getSample (c, s2)));
            }
        };
        juce::Array<juce::File> toDelete;

        {   // manual capture
            JMCReverseProcessor p;
            p.prepareToPlay (48000.0, 256);
            p.setPlayConfigDetails (2, 2, 48000.0, 256);
            p.startCapture();
            CHECK (p.isCapturing(), "capture starts");
            float passErr = 0;
            feed (p, 200, 0.5f, true, &passErr);
            CHECK (passErr == 0.0f, "capture does not alter the audio passing through");
            p.stopCapture();
            CHECK (! p.isCapturing() && waitFor (p, jmc::EngineState::Ready), "stop -> capture saved, analysed, rendered");
            auto src = p.getSource();
            const auto f = p.getSourceFile();
            toDelete.add (f);
            CHECK (f.existsAsFile() && f.getParentDirectory() == JMCReverseProcessor::capturesDir(), "capture stored in the captures folder");
            CHECK (src != nullptr && src->buffer.getNumSamples() == 200 * 256 && src->numChannels() == 2 && std::abs (src->sampleRate - 48000.0) < 0.5, "captured length/channels/rate correct");
            double err = 0;
            if (src != nullptr)
            {
                double ph = 0;
                for (int i = 0; i < 2000; ++i, ph += 0.05)
                    err = std::max (err, (double) std::abs (src->buffer.getSample (0, i) - 0.5f * (float) std::sin (ph)));
            }
            CHECK (err < 1e-6, "captured samples are bit-accurate (32-bit float WAV)");

            // Capturing while the preview plays must not record the preview.
            p.play();
            p.startCapture();
            feed (p, 100, 0.25f);
            p.stopCapture();
            waitFor (p, jmc::EngineState::Ready);
            float pk = 0; if (auto s2 = p.getSource()) pk = s2->buffer.getMagnitude (0, 0, s2->buffer.getNumSamples());
            toDelete.add (p.getSourceFile());
            std::printf ("      peak of 2nd capture (input 0.25 + preview playing): %.3f\n", pk);
            CHECK (std::abs (pk - 0.25f) < 1e-3f, "preview output is never captured");
        }
        {   // silent + too short
            JMCReverseProcessor p;
            p.prepareToPlay (48000.0, 256);
            p.setPlayConfigDetails (2, 2, 48000.0, 256);
            p.startCapture(); feed (p, 100, 0.0f, false); p.stopCapture();
            CHECK (p.getNote().contains ("silent"), "silent capture -> explanatory note, nothing loaded");
            p.startCapture(); feed (p, 5, 0.5f); p.stopCapture();
            CHECK (p.getNote().contains ("Nothing captured"), "too-short capture -> note");
            CHECK (p.getSource() == nullptr, "failed captures load nothing");
        }
        {   // auto-capture follows the host transport
            JMCReverseProcessor p;
            TestPlayHead ph;
            p.setPlayHead (&ph);
            p.prepareToPlay (48000.0, 256);
            p.setPlayConfigDetails (2, 2, 48000.0, 256);
            p.setAutoCapture (true);
            ph.playing = false; feed (p, 20, 0.5f);
            ph.playing = true;  feed (p, 150, 0.5f);
            ph.playing = false; feed (p, 5, 0.5f);
            p.pollCapture();
            CHECK (waitFor (p, jmc::EngineState::Ready), "auto-capture: transport stop finalises and loads the capture");
            auto src = p.getSource();
            toDelete.add (p.getSourceFile());
            std::printf ("      auto-captured %d samples (expected %d)\n", src ? src->buffer.getNumSamples() : -1, 150 * 256);
            CHECK (src != nullptr && src->buffer.getNumSamples() == 150 * 256, "auto-capture recorded exactly the playing span");
            // second pass replaces the first
            ph.playing = true;  feed (p, 60, 0.5f);
            ph.playing = false; feed (p, 2, 0.5f);
            p.pollCapture();
            waitFor (p, jmc::EngineState::Ready);
            toDelete.add (p.getSourceFile());
            CHECK (p.getSource() != nullptr && p.getSource()->buffer.getNumSamples() == 60 * 256, "a second playback pass captures anew");
            p.setPlayHead (nullptr);
        }
        {   // capture buffer full (60 s)
            JMCReverseProcessor p;
            p.prepareToPlay (48000.0, 256);
            p.setPlayConfigDetails (2, 2, 48000.0, 256);
            p.startCapture();
            feed (p, 11400, 0.3f);   // 60.8 s > 60 s capacity
            p.pollCapture();
            CHECK (! p.isCapturing() && waitFor (p, jmc::EngineState::Ready, 60000), "buffer full (60 s) auto-finalises");
            toDelete.add (p.getSourceFile());
            CHECK (p.getSource() != nullptr && p.getSource()->buffer.getNumSamples() == 60 * 48000, "capture capped at 60 s");
        }
        {   // loop + reset + FX toggles
            JMCReverseProcessor p;
            p.prepareToPlay (48000.0, 256);
            p.setPlayConfigDetails (2, 2, 48000.0, 256);
            p.loadFile (wav);
            waitFor (p, jmc::EngineState::Ready);
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            p.setLoop (false); p.play();
            float lastPeak = 0;
            for (int i = 0; i < 300; ++i) { buf.clear(); p.processBlock (buf, midi); if (i > 280) lastPeak = std::max (lastPeak, buf.getMagnitude (0, 256)); }
            CHECK (! p.isPlaying(), "without loop playback stops at the end");
            p.setLoop (true); p.play();
            lastPeak = 0;
            for (int i = 0; i < 400; ++i) { buf.clear(); p.processBlock (buf, midi); if (i > 380) lastPeak = std::max (lastPeak, buf.getMagnitude (0, 256)); }
            CHECK (p.isPlaying() && lastPeak > 0.0f, "loop keeps playing past the end of the audio");
            p.stop();

            p.applyCharacter (11);
            setReal (p, "decay", 9.0f);
            p.addSibRegion (0.1f, 0.2f);
            p.resetAll();
            bool defaults = true;
            for (auto* par : p.getParameters()) if (std::abs (par->getValue() - par->getDefaultValue()) > 1e-6f) defaults = false;
            CHECK (defaults && p.getCharacter() == 0 && p.getPresetName().isEmpty(), "Reset restores every default and clears character/preset");
            bool noEdits = true; for (auto& g : p.getSibRegions()) if (g.start > 0.09f && g.end < 0.21f && g.end - g.start < 0.11f) noEdits = false;
            CHECK (noEdits, "Reset clears manual sibilance edits");
            p.undo();
            CHECK (std::abs (real (p, "decay") - 9.0f) < 0.01f, "Reset can be undone");

            std::map<std::string, float> m;
            auto grab = [&] { for (auto* par : p.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (par)) m[r->getParameterID().toStdString()] = r->convertFrom0to1 (r->getValue()); };
            setReal (p, "chorus", 0); setReal (p, "rampon", 0); setReal (p, "tremolo", 0);
            grab();
            auto off = jmc::settingsFromParams ([&] (const char* id) { return m.at (id); }, 100.0);
            CHECK (off.modDepth == 0.0f && off.rampMix == 0.0f && off.tremMix == 0.0f, "FX toggles off -> no processing");
            setReal (p, "chorus", 1); setReal (p, "rampon", 1); setReal (p, "rampa", 2); setReal (p, "rampb", 0); setReal (p, "rampc", 4); setReal (p, "tremolo", 1); setReal (p, "tremrate", 2); setReal (p, "tremmix", 0.7f);
            grab();
            auto on = jmc::settingsFromParams ([&] (const char* id) { return m.at (id); }, 100.0);
            CHECK (on.modDepth > 0.0f && on.rampMix > 0.0f && std::abs (on.tremMix - 0.7f) < 1e-3f && std::abs (on.tremRate - 2.0f) < 1e-6f && std::abs (on.bpm - 100.0f) < 1e-3f,
                   "FX toggles on -> chorus/ramp tremolo/tremolo active, tremolo timing 1/8 and host tempo passed");
        }
        for (auto& f : toDelete) f.deleteFile();
    }


    // ---- Export + drag file: same audio as the FX preview
    {
        JMCReverseProcessor p;
        p.prepareToPlay (48000.0, 256);
        p.loadFile (wav);
        waitFor (p, jmc::EngineState::Ready);
        CHECK (p.isResultCurrent(), "result is current after rendering");
        setReal (p, "decay", 5.5f);
        CHECK (! p.isResultCurrent(), "changing a parameter makes the result stale immediately");
        juce::String err;
        CHECK (! p.exportResultNow (tmp.getChildFile ("stale.wav"), err) && err.contains ("rendering") && ! tmp.getChildFile ("stale.wav").exists(),
               "export refuses a stale result (never exports old audio)");
        CHECK (waitFor (p, jmc::EngineState::Ready) && [&] { for (int i = 0; i < 3000 && ! p.isResultCurrent(); ++i) { p.updateRenderIfNeeded(); juce::Thread::sleep (10); } return p.isResultCurrent(); }(),
               "result becomes current again after the re-render");

        p.setExportOptions ({ 32, 0.0, false });
        const auto out = tmp.getChildFile ("preview_match.wav");
        CHECK (p.exportResultNow (out, err), "export succeeds");
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatReader> rd (fmt.createReaderFor (out.createInputStream().release(), true));
        auto rendered = p.getRendered();
        bool same = rd != nullptr && rendered != nullptr && rd->lengthInSamples == rendered->buffer.getNumSamples();
        if (same)
        {
            juce::AudioBuffer<float> back (2, (int) rd->lengthInSamples);
            rd->read (&back, 0, (int) rd->lengthInSamples, 0, true, true);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < back.getNumSamples(); ++i) if (std::abs (back.getSample (c, i) - rendered->buffer.getSample (c, i)) > 0.0f) same = false;
        }
        CHECK (same, "exported file is sample-identical to what the FX preview plays");

        jmc::ExportOptions o { 24, 96000.0, true };
        p.setExportOptions (o);
        juce::MemoryBlock blk;
        p.getStateInformation (blk);
        JMCReverseProcessor q;
        q.setStateInformation (blk.getData(), (int) blk.getSize());
        CHECK (q.getExportOptions() == o, "export options persist in the project state");

        p.setExportOptions ({ 24, 0.0, false });
        const auto asyncOut = tmp.getChildFile ("async.wav");
        p.exportResultAsync (asyncOut);
        bool done = false;
        for (int i = 0; i < 500 && ! done; ++i) { done = p.getExportStatus().startsWith ("Exported"); juce::Thread::sleep (10); }
        std::printf ("      status: %s\n", p.getExportStatus().toRawUTF8());
        CHECK (done && asyncOut.existsAsFile() && p.getLastExportFile() == asyncOut && p.getExportStatus().contains ("24-bit"), "async export completes and reports");

        p.applyCharacter (2);
        waitFor (p, jmc::EngineState::Ready);
        for (int i = 0; i < 3000 && ! p.isResultCurrent(); ++i) { p.updateRenderIfNeeded(); juce::Thread::sleep (10); }
        waitFor (p, jmc::EngineState::Ready);
        const auto name = p.suggestedExportName();
        std::printf ("      suggested name: %s\n", name.toRawUTF8());
        CHECK (name.startsWith ("JMCReverse_VocalLift_") && name.contains ("bars_120bpm.wav"), "suggested file name follows JMCReverse_<Effect>_<N>bars_<bpm>bpm.wav");
        p.setExportOptions ({ 32, 0.0, false });
        const auto drag = p.writeDragFile (err);
        std::unique_ptr<juce::AudioFormatReader> rd2 (fmt.createReaderFor (drag.createInputStream().release(), true));
        auto rendered2 = p.getRendered();
        CHECK (drag.getFileName() == name && drag.getParentDirectory() == JMCReverseProcessor::dragDir() && rd2 != nullptr
                   && rendered2 != nullptr && rd2->lengthInSamples == rendered2->buffer.getNumSamples(), "drag file written to the temp folder with the same name and length");
        drag.deleteFile();
    }

    // ---- Click-to-play: SOURCE / RESULT waveforms play from the clicked position
    {
        JMCReverseProcessor p;
        p.prepareToPlay (48000.0, 256);
        p.setPlayConfigDetails (2, 2, 48000.0, 256);
        p.loadFile (wav);
        waitFor (p, jmc::EngineState::Ready);
        for (int i = 0; i < 3000 && ! p.isResultCurrent(); ++i) { p.updateRenderIfNeeded(); juce::Thread::sleep (10); }
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        auto run = [&] (int blocks) { float pk = 0; for (int i = 0; i < blocks; ++i) { buf.clear(); p.processBlock (buf, midi); pk = std::max (pk, buf.getMagnitude (0, 256)); } return pk; };

        p.playFrom (false, 0.5);
        const float pkSrc = run (4);
        const double h1 = p.getPlayheadNormalised();
        std::printf ("      click source at 0.50 -> playhead %.3f, peak %.3f\n", h1, pkSrc);
        CHECK (p.isPlaying() && ! p.isPreviewResult() && h1 > 0.49 && h1 < 0.56 && pkSrc > 0.0f, "clicking the source plays RAW from that position");

        p.playFrom (true, 0.25);   // a different buffer with a different length, while still playing
        const float pkFx = run (4);
        const double h2 = p.getPlayheadNormalised();
        std::printf ("      click result at 0.25 -> playhead %.3f, peak %.3f\n", h2, pkFx);
        CHECK (p.isPlaying() && p.isPreviewResult() && h2 > 0.24 && h2 < 0.27, "clicking the result switches to FX and starts at the clicked position (resolved against the result's own length)");

        p.playFrom (true, 0.9);
        run (2);
        const double h3 = p.getPlayheadNormalised();
        CHECK (h3 > 0.89 && h3 < 0.95, "clicking near the end of the result lands there");

        p.playFrom (false, 0.1);
        run (2);
        const double h4 = p.getPlayheadNormalised();
        CHECK (! p.isPreviewResult() && h4 > 0.09 && h4 < 0.14, "going back to the source jumps to its own 0.10");

        p.stop();
        run (40);
        CHECK (! p.isPlaying(), "double-click (stop) ends playback");

        JMCReverseProcessor empty;
        empty.prepareToPlay (48000.0, 256);
        empty.setPlayConfigDetails (2, 2, 48000.0, 256);
        empty.playFrom (true, 0.5);
        juce::AudioBuffer<float> b2 (2, 256);
        b2.clear();
        empty.processBlock (b2, midi);
        CHECK (b2.getMagnitude (0, 256) == 0.0f || std::abs (b2.getMagnitude (0, 256)) < 1e-9f, "clicking with nothing loaded is harmless and silent");
    }

    // ---- Audio thread safety smoke test: process blocks while rendering/loading repeatedly
    {
        JMCReverseProcessor p;
        p.prepareToPlay (48000.0, 256);
        p.setPlayConfigDetails (2, 2, 48000.0, 256);
        p.loadFile (wav);
        waitFor (p, jmc::EngineState::Ready);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        p.play();
        bool finite = true;
        for (int i = 0; i < 600; ++i)
        {
            buf.clear();
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) for (int s = 0; s < 256; ++s) if (! std::isfinite (buf.getSample (c, s))) finite = false;
            if (i % 50 == 0) { setReal (p, "length", 1.0f + (float) (i % 7)); p.requestRender(); }
            if (i == 300) p.loadFile (wav);
            if (i == 200) p.setPreviewResult (true);
            if (i == 400) p.setPreviewResult (false);
            if (i % 100 == 0) p.seekNormalised (0.3);
            juce::Thread::sleep (1);
        }
        CHECK (finite, "600 audio blocks during repeated renders/loads/seeks/A-B: all finite, no crash");
        p.stop();
    }

    tmp.deleteRecursively();
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}

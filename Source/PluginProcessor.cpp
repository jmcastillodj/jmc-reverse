#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "audio/Resampler.h"
#include "dsp/Timing.h"
#include "Characters.h"
#include "SettingsFromParams.h"
#include "ExportNaming.h"

using namespace jmc;

JMCReverseProcessor::JMCReverseProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "JMCREVERSE", createLayout())
{
    clearUndoHistory();
}

JMCReverseProcessor::~JMCReverseProcessor()
{
    stopTimer();
    capState.store (0);
    {
        const juce::ScopedLock sl (lock);
        ++generation; // invalidate anything in flight
    }
    pool.removeAllJobs (true, 20000);
    playback.store (nullptr);
    rendered.store (nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout JMCReverseProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto add = [&] (const char* id, const char* name, R range, float def, const char* unit)
    {
        // Readable values (no 7-digit floats): decimals follow the unit / range.
        const std::string u (unit);
        const int decimals = (u == "Hz" || u == "ms") ? 0 : (u == "dB" || u == "st") ? 1 : 2;
        l.add (std::make_unique<FP> (juce::ParameterID { id, 1 }, name, range, def,
                                     Attr().withLabel (unit).withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); })));
    };
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::mode, 1 }, "Swell Mode",
                                                         juce::StringArray { "Smooth tail", "Phrase" }, 1));
    add (ids::length,   "Length",     R (0.25f, 16.0f, 0.0f, 0.5f),    4.0f,    "s");
    add (ids::decay,    "Decay",      R (0.1f, 20.0f, 0.0f, 0.5f),     2.5f,    "s");
    add (ids::size,     "Size",       R (0.25f, 2.0f),                 1.0f,    "x");
    add (ids::predelay, "Pre-delay",  R (0.0f, 250.0f, 0.0f, 0.5f),    0.0f,    "ms");
    add (ids::damping,  "Damping",    R (0.0f, 1.0f),                  0.4f,    "");
    add (ids::lowcut,   "Low Cut",    R (20.0f, 2000.0f, 0.0f, 0.3f),  20.0f,   "Hz");
    add (ids::highcut,  "High Cut",   R (1000.0f, 20000.0f, 0.0f, 0.3f), 20000.0f, "Hz");
    add (ids::width,    "Width",      R (0.0f, 2.0f),                  1.0f,    "");
    add (ids::dry,      "Dry",        R (0.0f, 1.0f),                  0.0f,    "");
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::sync, 1 }, "Tempo Sync", false));
    {
        juce::StringArray names;
        for (auto* n : timing::divisionNames) names.add (n);
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::division, 1 }, "Division", names, 4));
    }
    add (ids::preroll,  "Pre-roll",   R (0.0f, 4.0f, 0.0f, 0.5f),      0.0f,    "s");
    add (ids::tail,     "Tail",       R (0.0f, 8.0f, 0.0f, 0.5f),      0.0f,    "s");
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::sibon, 1 }, "Sibilance Detection", false));
    add (ids::sibsens,  "Sibilance Sensitivity", R (0.0f, 1.0f),      0.5f,    "");
    add (ids::sibamt,   "Sibilance Amount",       R (0.0f, 1.0f),      0.7f,    "");
    add (ids::lowdamp,  "Low Damping", R (0.0f, 1.0f),                 0.2f,    "");
    add (ids::tilt,     "Tilt",       R (-8.0f, 8.0f),                 0.0f,    "dB");
    add (ids::balance,  "Balance",    R (-1.0f, 1.0f),                 0.0f,    "");
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::curve, 1 }, "Fade Curve",
                                                         juce::StringArray { "S-curve", "Linear", "Exponential", "Logarithmic" }, 0));
    add (ids::pitch,    "Pitch Movement", R (-12.0f, 12.0f),           0.0f,    "st");
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::chorus, 1 }, "Chorus", false));
    add (ids::moddepth, "Chorus Mix", R (0.0f, 1.0f),                  0.5f,    "");
    {
        const juce::StringArray notes { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
        juce::StringArray mid { "Auto" };
        mid.addArray (notes);
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::rampon, 1 }, "Tremolo Ramp", false));
        add (ids::rampmix, "Ramp Mix", R (0.0f, 1.0f), 1.0f, "");
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::rampa, 1 }, "Ramp Start", notes, 2));
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::rampb, 1 }, "Ramp Mid", mid, 0));
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::rampc, 1 }, "Ramp End", notes, 4));
    }
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::tremolo, 1 }, "Tremolo", false));
    add (ids::tremmix,  "Tremolo Mix", R (0.0f, 1.0f),                 1.0f,    "");
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::tremrate, 1 }, "Tremolo Timing",
                                                         juce::StringArray { "1/2", "1/4", "1/8", "1/16", "1/32" }, 2));
    add (ids::modrate,  "Mod Rate",   R (0.1f, 8.0f, 0.0f, 0.5f),      0.8f,    "Hz");
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::gaterate, 1 }, "Stutter Rate",
                                                         juce::StringArray { "Off", "1/8", "1/16", "1/32" }, 0));
    add (ids::gatedepth, "Stutter Depth", R (0.0f, 1.0f),              0.8f,    "");
    add (ids::gateramp, "Stutter Ramp", R (0.0f, 1.0f),                0.0f,    "");
    add (ids::drive,    "Saturation", R (0.0f, 1.0f),                  0.0f,    "");
    add (ids::fadein,   "Fade In",    R (0.0f, 8.0f, 0.0f, 0.4f),      0.05f,   "s");
    add (ids::fadeout,  "Fade Out",   R (0.0f, 2.0f, 0.0f, 0.4f),      0.005f,  "s");
    add (ids::outgain,  "Output Gain", R (-24.0f, 12.0f),              0.0f,    "dB");
    add (ids::trimstart, "Trim Start", R (0.0f, 1.0f),                 0.0f,    "");
    add (ids::trimend,   "Trim End",   R (0.0f, 1.0f),                 1.0f,    "");
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::autolevel, 1 }, "Auto Level", true));
    return l;
}

RenderSettings JMCReverseProcessor::readSettings()
{
    auto s = settingsFromParams ([this] (const char* id) { return apvts.getRawParameterValue (id)->load(); }, hostBpm.load());
    if (*apvts.getRawParameterValue (ids::sibon) > 0.5f)
    {
        const juce::ScopedLock sl (lock);
        ensureSib (*apvts.getRawParameterValue (ids::sibsens));
        s.sibAmount = *apvts.getRawParameterValue (ids::sibamt);
        s.sibRegions = sibFinal;
    }
    return s;
}

bool JMCReverseProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

juce::String JMCReverseProcessor::getErrorMessage() const
{
    const juce::ScopedLock sl (lock);
    return errorMessage;
}

std::shared_ptr<const LoadedAudio> JMCReverseProcessor::getSource() const
{
    const juce::ScopedLock sl (lock);
    return source;
}

std::shared_ptr<const PlaybackAudio> JMCReverseProcessor::getRendered() const
{
    const juce::ScopedLock sl (lock);
    return renderedOwner;
}

// ---------------------------------------------------------------- export
bool JMCReverseProcessor::isResultCurrent()
{
    const auto cur = readSettings();
    const juce::ScopedLock sl (lock);
    return state.load() == EngineState::Ready && renderedOwner != nullptr && cur == lastRequested;
}

juce::String JMCReverseProcessor::suggestedExportName()
{
    juce::String effect = getPresetName();
    if (effect.isEmpty())
        if (const int c = getCharacter(); c >= 1 && c <= characters::count)
            effect = characters::names[c - 1];
    return makeExportName (effect, statRiserOnly.load(), hostBpm.load(), tsNum.load(), tsDen.load());
}

ExportOptions JMCReverseProcessor::getExportOptions() const
{
    ExportOptions o;
    o.bits = (int) apvts.state.getProperty ("exportBits", 24) == 32 ? 32 : 24;
    o.sampleRate = (double) apvts.state.getProperty ("exportRate", 0.0);
    o.mono = (bool) apvts.state.getProperty ("exportMono", false);
    return o;
}

void JMCReverseProcessor::setExportOptions (const ExportOptions& o)
{
    apvts.state.setProperty ("exportBits", o.bits, nullptr);
    apvts.state.setProperty ("exportRate", o.sampleRate, nullptr);
    apvts.state.setProperty ("exportMono", o.mono, nullptr);
}

juce::String JMCReverseProcessor::getExportStatus() const
{
    const juce::ScopedLock sl (lock);
    return exportStatus;
}

juce::File JMCReverseProcessor::getLastExportFile() const
{
    const juce::ScopedLock sl (lock);
    return lastExport;
}

static juce::String describe (const ExportResult& r)
{
    return juce::String (r.channels == 1 ? "mono" : "stereo") + ", " + juce::String (r.sampleRate / 1000.0, 1) + " kHz";
}

bool JMCReverseProcessor::exportResultNow (const juce::File& dest, juce::String& error)
{
    std::shared_ptr<PlaybackAudio> res;
    {
        const juce::ScopedLock sl (lock);
        res = renderedOwner;
    }
    if (res == nullptr || ! isResultCurrent())
    {
        error = "The effect is still rendering. Wait for \"Ready\" and try again.";
        return false;
    }
    const auto r = exportWav (dest, res->buffer, res->sampleRate, getExportOptions());
    error = r.error;
    return r.ok;
}

void JMCReverseProcessor::exportResultAsync (const juce::File& dest)
{
    std::shared_ptr<PlaybackAudio> res;
    {
        const juce::ScopedLock sl (lock);
        res = renderedOwner;
        exportStatus = "Exporting...";
    }
    exportRevision.fetch_add (1);
    if (res == nullptr || ! isResultCurrent())
    {
        const juce::ScopedLock sl (lock);
        exportStatus = "Export failed: the effect is still rendering. Wait for \"Ready\" and try again.";
        exportRevision.fetch_add (1);
        return;
    }
    const auto opts = getExportOptions();
    pool.addJob ([this, res, dest, opts]
    {
        const auto r = exportWav (dest, res->buffer, res->sampleRate, opts);
        {
            const juce::ScopedLock sl (lock);
            if (r.ok)
            {
                lastExport = dest;
                exportStatus = "Exported " + dest.getFileName() + " (" + juce::String (opts.bits == 32 ? "32-bit float" : "24-bit") + ", " + describe (r) + ")"
                               + (r.peakScale < 1.0f ? " - level reduced " + juce::String (-20.0 * std::log10 ((double) r.peakScale), 1) + " dB to avoid clipping" : juce::String());
            }
            else
                exportStatus = "Export failed: " + r.error;
        }
        exportRevision.fetch_add (1);
    });
}

juce::File JMCReverseProcessor::dragDir()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("JMC REVERSE Drag");
}

juce::File JMCReverseProcessor::writeDragFile (juce::String& error)
{
    auto f = dragDir().getChildFile (suggestedExportName());
    return exportResultNow (f, error) ? f : juce::File();
}

// ---------------------------------------------------------------- capture / reset
juce::File JMCReverseProcessor::capturesDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("JMC").getChildFile ("JMC REVERSE").getChildFile ("Captures");
}

juce::String JMCReverseProcessor::getNote() const
{
    const juce::ScopedLock sl (lock);
    return note;
}

void JMCReverseProcessor::setAutoCapture (bool on)
{
    if (on && capBuf.load() == nullptr)
    {
        capBufOwner = std::make_unique<juce::AudioBuffer<float>> (2, (int) (60.0 * hostRate.load()));
        capBufOwner->clear();
        capBuf.store (capBufOwner.get(), std::memory_order_release);
    }
    autoCapture.store (on);
    if (on)
        startTimerHz (10);
}

void JMCReverseProcessor::startCapture()
{
    if (capBuf.load() == nullptr)
    {
        capBufOwner = std::make_unique<juce::AudioBuffer<float>> (2, (int) (60.0 * hostRate.load())); // 60 s
        capBufOwner->clear();
        capBuf.store (capBufOwner.get(), std::memory_order_release);
    }
    stop();
    capFinalize.store (false);
    capPos.store (0);
    capNumCh.store (juce::jmax (1, juce::jmin (2, getTotalNumInputChannels())));
    {
        const juce::ScopedLock sl (lock);
        note.clear();
    }
    capState.store (1);
    startTimerHz (10);
}

void JMCReverseProcessor::stopCapture()
{
    for (int i = 0; i < 500; ++i) // wait (max ~0.5 s) for an in-flight audio block to finish its write
    {
        int expected = 1;
        if (capState.compare_exchange_strong (expected, 3))
            break;
        if (capState.load() != 2)
            break;
        juce::Thread::sleep (1);
    }
    if (capState.load() == 3)
        finalizeCapture();
}

void JMCReverseProcessor::pollCapture()
{
    if (capFinalize.exchange (false) && capState.load() == 3)
        finalizeCapture();
}

void JMCReverseProcessor::finalizeCapture()
{
    auto* cb = capBuf.load();
    const int n = juce::jmin (capPos.load(), cb != nullptr ? cb->getNumSamples() : 0);
    const int nch = capNumCh.load();
    capState.store (0);
    capFinalize.store (false);
    const double sr = hostRate.load();

    auto setNote = [this] (const juce::String& s) { const juce::ScopedLock sl (lock); note = s; };
    if (cb == nullptr || n < (int) (0.1 * sr))
    {
        setNote ("Nothing captured (need at least 0.1 s of audio).");
        return;
    }
    float peak = 0.0f;
    for (int c = 0; c < nch; ++c)
        peak = juce::jmax (peak, cb->getMagnitude (c, 0, n));
    if (peak < 1.0e-6f)
    {
        setNote ("The captured audio is silent - is the track playing through the plugin?");
        return;
    }
    auto dir = capturesDir();
    if (! dir.createDirectory().wasOk())
    {
        setNote ("Could not create the captures folder.");
        return;
    }
    auto file = dir.getChildFile ("Capture_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + juce::String::formatted ("_%03d", (int) (juce::Time::getMillisecondCounter() % 1000)) + ".wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (file.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w;
    if (os != nullptr)
        w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (nch).withBitsPerSample (32));
    if (w == nullptr || ! w->writeFromAudioSampleBuffer (*cb, 0, n))
    {
        setNote ("Could not write the capture file.");
        return;
    }
    w.reset();
    setNote ("Captured " + juce::String ((double) n / sr, 2) + " s");
    loadFile (file);
}

void JMCReverseProcessor::resetAll()
{
    commitUndoNow();
    stop();
    for (auto* p : getParameters())
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
    clearSibEdits();
    apvts.state.setProperty ("character", 0, nullptr);
    apvts.state.setProperty ("preset", "", nullptr);
}

// ---------------------------------------------------------------- undo / redo
JMCReverseProcessor::Snap JMCReverseProcessor::takeSnap() const
{
    Snap s;
    for (auto* p : getParameters())
        s.values.push_back (p->getValue());
    s.character = getCharacter();
    s.preset = getPresetName();
    return s;
}

bool JMCReverseProcessor::sameSnap (const Snap& a, const Snap& b) const
{
    if (a.values.size() != b.values.size() || a.character != b.character || a.preset != b.preset)
        return false;
    for (size_t i = 0; i < a.values.size(); ++i)
        if (std::abs (a.values[i] - b.values[i]) > 1.0e-6f)
            return false;
    return true;
}

void JMCReverseProcessor::restoreSnap (const Snap& s)
{
    auto params = getParameters();
    for (int i = 0; i < params.size() && (size_t) i < s.values.size(); ++i)
    {
        params[i]->beginChangeGesture();
        params[i]->setValueNotifyingHost (s.values[(size_t) i]);
        params[i]->endChangeGesture();
    }
    apvts.state.setProperty ("character", s.character, nullptr);
    apvts.state.setProperty ("preset", s.preset, nullptr);
}

void JMCReverseProcessor::clearUndoHistory()
{
    undoStack.clear();
    redoStack.clear();
    lastCommitted = lastSeenSnap = takeSnap();
    snapInit = true;
}

void JMCReverseProcessor::commitUndoNow()
{
    if (! snapInit) { clearUndoHistory(); return; }
    const auto cur = takeSnap();
    if (! sameSnap (cur, lastCommitted))
    {
        undoStack.push_back (lastCommitted);
        if (undoStack.size() > 100) undoStack.erase (undoStack.begin());
        redoStack.clear();
        lastCommitted = lastSeenSnap = cur;
    }
}

void JMCReverseProcessor::commitUndoIfStable()
{
    const auto cur = takeSnap();
    const auto now = juce::Time::getMillisecondCounter();
    if (! snapInit) { clearUndoHistory(); return; }
    if (! sameSnap (cur, lastSeenSnap)) { lastSeenSnap = cur; snapChangeMs = now; return; }
    if (! sameSnap (cur, lastCommitted) && now - snapChangeMs > 300)
    {
        undoStack.push_back (lastCommitted);
        if (undoStack.size() > 100) undoStack.erase (undoStack.begin());
        redoStack.clear();
        lastCommitted = cur;
    }
}

void JMCReverseProcessor::undo()
{
    const auto cur = takeSnap();
    if (snapInit && ! sameSnap (cur, lastCommitted)) // flush an edit that has not settled yet
    {
        undoStack.push_back (lastCommitted);
        redoStack.clear();
        lastCommitted = cur;
    }
    if (undoStack.empty()) return;
    redoStack.push_back (lastCommitted);
    const auto target = undoStack.back();
    undoStack.pop_back();
    restoreSnap (target);
    lastCommitted = lastSeenSnap = target;
}

void JMCReverseProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (lastCommitted);
    const auto target = redoStack.back();
    redoStack.pop_back();
    restoreSnap (target);
    lastCommitted = lastSeenSnap = target;
}

// ---------------------------------------------------------------- characters
void JMCReverseProcessor::applyValues (const std::vector<std::pair<juce::String, float>>& values, const juce::String& presetName)
{
    commitUndoNow();
    for (const auto& [id, value] : values)
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    apvts.state.setProperty ("preset", presetName, nullptr);
    apvts.state.setProperty ("character", 0, nullptr);
}

void JMCReverseProcessor::applyCharacter (int oneBased)
{
    if (oneBased < 1 || oneBased > characters::count)
        return;
    std::vector<std::pair<juce::String, float>> v;
    for (const auto& [id, value] : characters::resolve (oneBased - 1))
        v.emplace_back (id, value);
    applyValues (v, characters::names[oneBased - 1]);
    apvts.state.setProperty ("preset", "", nullptr);
    apvts.state.setProperty ("character", oneBased, nullptr);
}

bool JMCReverseProcessor::isCharacterModified() const
{
    const int c = getCharacter();
    if (c < 1 || c > characters::count)
        return false;
    for (const auto& [id, value] : characters::resolve (c - 1))
        if (auto* p = apvts.getParameter (id))
            if (std::abs (p->convertFrom0to1 (p->getValue()) - value) > 1.0e-3f * std::max (1.0f, std::abs (value)))
                return true;
    return false;
}

// ---------------------------------------------------------------- sibilance
// Call with `lock` held. Re-runs only the cheap threshold stage, and only when an input changed.
void JMCReverseProcessor::ensureSib (float sens)
{
    if (sibFeatures != nullptr && std::abs (sens - sibKeySens) < 1e-6f && sibKeyEdits == sibEditRev && sibKeyFeat == sibFeatRev)
        return;
    sibKeySens = sens;
    sibKeyEdits = sibEditRev;
    sibKeyFeat = sibFeatRev;

    std::vector<SibRegion> regions;
    juce::String msg;
    if (sibFeatures == nullptr)
        msg = "Load a file to detect sibilance.";
    else
    {
        auto res = detectSibilance (*sibFeatures, sens);
        regions = res.regions;
        msg = res.message;
        for (const auto& e : sibEdits)
        {
            if (e.add)
                regions.push_back ({ e.start, e.end });
            else
                regions.erase (std::remove_if (regions.begin(), regions.end(),
                                               [&] (const SibRegion& g) { return g.start < e.end && g.end > e.start; }),
                               regions.end());
        }
        std::sort (regions.begin(), regions.end(), [] (const SibRegion& x, const SibRegion& y) { return x.start < y.start; });
        std::vector<SibRegion> merged;
        for (const auto& g : regions)
            if (! merged.empty() && g.start <= merged.back().end)
                merged.back().end = std::max (merged.back().end, g.end);
            else
                merged.push_back (g);
        regions = merged;
        if (! sibEdits.empty())
            msg += " (manual edits applied)";
    }
    if (! (regions == sibFinal) || msg != sibMsg)
    {
        sibFinal = regions;
        sibMsg = msg;
        sibRevision.fetch_add (1);
    }
}

std::vector<SibRegion> JMCReverseProcessor::getSibRegions()
{
    const juce::ScopedLock sl (lock);
    ensureSib (*apvts.getRawParameterValue (ids::sibsens));
    return sibFinal;
}

juce::String JMCReverseProcessor::getSibMessage()
{
    const juce::ScopedLock sl (lock);
    ensureSib (*apvts.getRawParameterValue (ids::sibsens));
    return sibMsg;
}

void JMCReverseProcessor::addSibRegion (float a, float b)
{
    const juce::ScopedLock sl (lock);
    if (b - a > 1e-4f) { sibEdits.push_back ({ true, std::max (0.0f, a), std::min (1.0f, b) }); ++sibEditRev; }
}

void JMCReverseProcessor::removeSibRegionAt (float at)
{
    const juce::ScopedLock sl (lock);
    sibEdits.push_back ({ false, at, at + 1e-6f });
    ++sibEditRev;
}

void JMCReverseProcessor::clearSibEdits()
{
    const juce::ScopedLock sl (lock);
    sibEdits.clear();
    ++sibEditRev;
}

// ---------------------------------------------------------------- loading
static std::shared_ptr<PlaybackAudio> makePlayback (const LoadedAudio& src, double rate, const std::function<bool()>& cancel)
{
    auto pb = std::make_shared<PlaybackAudio>();
    pb->sampleRate = rate;
    const int nch = src.buffer.getNumChannels();
    const int n = src.buffer.getNumSamples();
    for (int c = 0; c < nch; ++c)
    {
        if (cancel())
            return nullptr;
        auto r = resampleSinc (src.buffer.getReadPointer (c), (size_t) n, src.sampleRate, rate);
        if (c == 0)
            pb->buffer.setSize (nch, (int) r.size());
        std::copy (r.begin(), r.end(), pb->buffer.getWritePointer (c));
    }
    return pb;
}

juce::File JMCReverseProcessor::getSourceFile() const
{
    const juce::ScopedLock sl (lock);
    return sourceFile;
}

void JMCReverseProcessor::loadFile (const juce::File& file, bool resetTrim)
{
    juce::uint32 gen;
    {
        const juce::ScopedLock sl (lock);
        gen = ++generation;
        errorMessage.clear();
        sourceFile = file;
    }
    stop();
    if (resetTrim)
        for (auto* id : { ids::trimstart, ids::trimend }) // a new file starts untrimmed
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->getDefaultValue());
    state.store (EngineState::Loading);

    pool.addJob ([this, gen, file]
    {
        auto isStale = [this, gen] { const juce::ScopedLock sl (lock); return generation != gen; };

        auto res = loadAudioFile (file, isStale);
        if (res.cancelled || isStale())
            return;
        if (! res.ok())
        {
            const juce::ScopedLock sl (lock);
            if (generation == gen)
            {
                errorMessage = res.error;
                state.store (EngineState::Error);
            }
            return;
        }

        state.store (EngineState::Analyzing);
        std::shared_ptr<const LoadedAudio> src = res.audio;
        const float* chs[2] = { src->buffer.getReadPointer (0), src->buffer.getReadPointer (src->buffer.getNumChannels() > 1 ? 1 : 0) };
        std::shared_ptr<const SibFeatures> feats = analyseSibilance (chs, src->buffer.getNumChannels(), (size_t) src->buffer.getNumSamples(),
                                                                     src->sampleRate, isStale);
        if (feats == nullptr)
            return;
        auto pb = makePlayback (*src, hostRate.load(), isStale);
        if (pb == nullptr)
            return;
        publish (gen, src, pb, true, feats);
    });
}

void JMCReverseProcessor::submitRebuild (juce::uint32 gen, std::shared_ptr<const LoadedAudio> src)
{
    pool.addJob ([this, gen, src]
    {
        auto isStale = [this, gen] { const juce::ScopedLock sl (lock); return generation != gen; };
        if (auto pb = makePlayback (*src, hostRate.load(), isStale))
            publish (gen, src, pb, false);
    });
}

void JMCReverseProcessor::publish (juce::uint32 gen, std::shared_ptr<const LoadedAudio> src,
                                   std::shared_ptr<PlaybackAudio> pb, bool newSource,
                                   std::shared_ptr<const SibFeatures> feats)
{
    {
        const juce::ScopedLock sl (lock);
        if (generation != gen)
            return;
        if (newSource)
        {
            source = src;
            sourceRevision.fetch_add (1);
            sibFeatures = std::move (feats);
            sibEdits = std::move (pendingSibEdits);
            pendingSibEdits.clear();
            ++sibEditRev;
            ++sibFeatRev;
            retire (std::move (renderedOwner)); // old result belongs to the previous file
            renderedOwner.reset();
            rendered.store (nullptr);
            renderRevision.fetch_add (1);
        }
        retire (std::move (playbackOwner));
        playbackOwner = pb;
        playback.store (pb.get(), std::memory_order_release);
    }
    requestRender();
}

void JMCReverseProcessor::retire (std::shared_ptr<PlaybackAudio> old)
{
    if (old != nullptr)
        graveyard.push_back ({ std::move (old), juce::Time::getMillisecondCounter() });
}

// The audio thread may still hold a retired pointer for the length of one block;
// keeping it alive for several seconds makes that safe without locking the audio thread.
void JMCReverseProcessor::collectGarbage()
{
    const juce::ScopedLock sl (lock);
    const auto now = juce::Time::getMillisecondCounter();
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                     [now] (const Retired& r) { return now - r.ms > 5000; }),
                     graveyard.end());
}

// ---------------------------------------------------------------- rendering
void JMCReverseProcessor::requestRender()
{
    std::shared_ptr<PlaybackAudio> in;
    RenderSettings s;
    juce::uint32 srcGen, rGen;
    {
        const juce::ScopedLock sl (lock);
        if (playbackOwner == nullptr)
            return;
        in = playbackOwner;
        srcGen = generation;
        rGen = ++renderGen;
        s = readSettings();
        lastRequested = s;
        state.store (EngineState::Rendering);
    }

    pool.addJob ([this, in, s, srcGen, rGen]
    {
        juce::ScopedNoDenormals noDenormals;
        auto isStale = [this, srcGen, rGen] { const juce::ScopedLock sl (lock); return generation != srcGen || renderGen != rGen; };
        if (isStale())
            return;

        const float* ptrs[2] = { in->buffer.getReadPointer (0),
                                 in->buffer.getReadPointer (in->buffer.getNumChannels() > 1 ? 1 : 0) };
        auto res = renderReverseReverb (ptrs, in->buffer.getNumChannels(), (size_t) in->buffer.getNumSamples(),
                                        in->sampleRate, s, isStale, &renderCache);
        if (res.cancelled || isStale())
            return;
        if (! res.ok)
        {
            const juce::ScopedLock sl (lock);
            if (generation == srcGen && renderGen == rGen)
            {
                errorMessage = "Render failed.";
                state.store (EngineState::Error);
            }
            return;
        }

        auto out = std::make_shared<PlaybackAudio>();
        out->sampleRate = in->sampleRate;
        out->buffer.setSize (2, (int) res.length());
        for (int c = 0; c < 2; ++c)
            std::copy (res.ch[c].begin(), res.ch[c].end(), out->buffer.getWritePointer (c));

        const juce::ScopedLock sl (lock);
        if (generation != srcGen || renderGen != rGen)
            return; // stale: a newer request or file has superseded this result
        retire (std::move (renderedOwner));
        renderedOwner = out;
        rendered.store (out.get(), std::memory_order_release);
        riserSeconds.store ((double) res.onsetSamples / in->sampleRate);
        statRiserOnly.store ((double) res.riserSamples / in->sampleRate);
        statCorr.store (res.correlation);
        statLoss.store (res.monoLossDb);
        statWidth.store (res.effectiveWidth);
        renderRevision.fetch_add (1);
        state.store (EngineState::Ready);
    });
}

void JMCReverseProcessor::updateRenderIfNeeded()
{
    const auto cur = readSettings();
    const auto now = juce::Time::getMillisecondCounter();
    if (! (cur == lastSeen))
    {
        lastSeen = cur;
        lastChangeMs = now;
        return;
    }
    bool need;
    {
        const juce::ScopedLock sl (lock);
        need = playbackOwner != nullptr && ! (cur == lastRequested);
    }
    if (need && now - lastChangeMs > 150)
        requestRender();
}

// ---------------------------------------------------------------- transport
void JMCReverseProcessor::seekNormalised (double p)
{
    seekNorm.store ((float) juce::jlimit (0.0, 1.0, p));
}

void JMCReverseProcessor::playFrom (bool result, double p)
{
    previewMode.store (result ? 1 : 0);
    seekNorm.store ((float) juce::jlimit (0.0, 1.0, p));
    wantPlay.store (true);
}

double JMCReverseProcessor::getPlayheadNormalised() const
{
    const int len = playLength.load();
    return len > 0 ? juce::jlimit (0.0, 1.0, double (playPos.load()) / len) : 0.0;
}

void JMCReverseProcessor::prepareToPlay (double sr, int)
{
    if (std::abs (hostRate.load() - sr) > 0.5 && capBuf.load() != nullptr)
    {
        capState.store (0);               // a recording made at another rate is discarded
        capBuf.store (nullptr);
        capBufOwner.reset();
    }
    hostRate.store (sr);
    std::shared_ptr<const LoadedAudio> src;
    juce::uint32 gen = 0;
    {
        const juce::ScopedLock sl (lock);
        src = source;
        gen = generation;
        if (playbackOwner != nullptr && std::abs (playbackOwner->sampleRate - sr) < 0.5)
            src.reset();
    }
    if (src != nullptr)
        submitRebuild (gen, src);
}

void JMCReverseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Host tempo / time signature (read-only; the host's timing is never modified).
    if (auto* ph = getPlayHead())
        if (auto position = ph->getPosition())
        {
            if (auto bpm = position->getBpm())
            {
                const double b = timing::sanitiseBpm (*bpm);
                if (std::abs (b - hostBpm.load (std::memory_order_relaxed)) > 0.004)
                    hostBpm.store (b);
                hostBpmSeen.store (true, std::memory_order_relaxed);
            }
            if (auto ts = position->getTimeSignature())
            {
                tsNum.store (ts->numerator, std::memory_order_relaxed);
                tsDen.store (ts->denominator, std::memory_order_relaxed);
            }
        }

    // Capture the incoming audio (before any preview is added). Lock-free handshake with the
    // message thread through capState; the buffer is allocated up front on the message thread.
    if (auto* cb = capBuf.load (std::memory_order_acquire))
    {
        bool playing = wasPlaying;
        if (auto* ph = getPlayHead())
            if (auto position = ph->getPosition())
                playing = position->getIsPlaying();
        if (autoCapture.load (std::memory_order_relaxed))
        {
            if (playing && ! wasPlaying && ! capFinalize.load())
            {
                capPos.store (0);
                capNumCh.store (juce::jmin (2, buffer.getNumChannels()));
                capState.store (1);
            }
            else if (! playing && wasPlaying)
            {
                int expected = 1;
                if (capState.compare_exchange_strong (expected, 3))
                    capFinalize.store (true);
            }
        }
        wasPlaying = playing;

        int expected = 1;
        if (capState.compare_exchange_strong (expected, 2))
        {
            const int p = capPos.load();
            const int n = juce::jmin (buffer.getNumSamples(), cb->getNumSamples() - p);
            const int nch = juce::jmin (capNumCh.load(), buffer.getNumChannels());
            for (int c = 0; c < cb->getNumChannels(); ++c)
                cb->copyFrom (c, p, buffer.getReadPointer (juce::jmin (c, nch - 1)), n);
            capPos.store (p + n);
            if (p + n >= cb->getNumSamples())
            {
                capState.store (3);
                capFinalize.store (true);
            }
            else
                capState.store (1);
        }
    }

    // Preview audio is summed on top of the (untouched) input.
    auto* pb = previewMode.load (std::memory_order_relaxed) == 1 ? rendered.load (std::memory_order_acquire)
                                                                 : playback.load (std::memory_order_acquire);
    const bool want = wantPlay.load();
    const float seekN = seekNorm.exchange (-1.0f);

    if (pb != lastPb)
    {
        const int oldLen = lastPb != nullptr ? lastPb->buffer.getNumSamples() : 0;
        const int newLen = pb != nullptr ? pb->buffer.getNumSamples() : 0;
        pos = (oldLen > 0 && newLen > 0) ? int (double (pos) * newLen / oldLen) : 0; // keep relative position
        lastPb = pb;
        if (oldLen == 0)
            active = false;
    }
    if (pb == nullptr || pb->buffer.getNumSamples() == 0)
    {
        active = false;
        pos = 0;
        playPos.store (0);
        playLength.store (0);
        return;
    }

    const int len = pb->buffer.getNumSamples();
    playLength.store (len);
    if (seekN >= 0.0f)
    {
        pos = juce::jlimit (0, len - 1, (int) ((double) seekN * (double) len));
        env = 0.0f; // fade back in after a jump to avoid a click
    }
    if (want && ! active)
    {
        active = true;
        if (pos >= len - 1)
            pos = 0;
        env = 0.0f;
    }
    if (! active)
    {
        playPos.store (pos);
        return;
    }

    const float step = 1.0f / float (0.005 * getSampleRate()); // 5 ms ramps
    const int nOut = juce::jmin (buffer.getNumChannels(), 2);
    const int nSrc = pb->buffer.getNumChannels();
    const int n = buffer.getNumSamples();

    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        env = want ? juce::jmin (1.0f, env + step) : juce::jmax (0.0f, env - step);
        if (! want && env <= 0.0f)
        {
            active = false;
            pos = 0;
            break;
        }
        if (pos >= len)
        {
            if (loop.load (std::memory_order_relaxed)) { pos = 0; env = 0.0f; continue; } // restart with a short fade-in
            active = false;
            wantPlay.store (false);
            pos = 0;
            break;
        }
        const float endGain = juce::jmin (1.0f, float (len - pos) * step);
        const float g = env * endGain;
        for (int c = 0; c < nOut; ++c)
        {
            float v;
            if (nOut == 1 && nSrc == 2)
                v = 0.5f * (pb->buffer.getSample (0, pos) + pb->buffer.getSample (1, pos));
            else
                v = pb->buffer.getSample (juce::jmin (c, nSrc - 1), pos);
            buffer.getWritePointer (c)[i] += v * g;
            pk = juce::jmax (pk, std::abs (v * g));
        }
        ++pos;
    }
    playPos.store (pos);
    if (pk > previewPeak.load (std::memory_order_relaxed))
        previewPeak.store (pk, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* JMCReverseProcessor::createEditor() { return new JMCReverseEditor (*this); }

// Project state: all parameters (APVTS), the chosen character/preset names, the path of the
// source audio and any manual sibilance edits. Audio itself is never embedded.
void JMCReverseProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("stateVersion", 1, nullptr);
    st.setProperty ("autoCapture", autoCapture.load(), nullptr);
    st.removeChild (st.getChildWithName ("SIBEDITS"), nullptr);
    {
        const juce::ScopedLock sl (lock);
        if (sourceFile != juce::File())
            st.setProperty ("sourcePath", sourceFile.getFullPathName(), nullptr);
        if (! sibEdits.empty())
        {
            juce::ValueTree edits ("SIBEDITS");
            for (const auto& e : sibEdits)
            {
                juce::ValueTree n ("E");
                n.setProperty ("add", e.add, nullptr);
                n.setProperty ("start", e.start, nullptr);
                n.setProperty ("end", e.end, nullptr);
                edits.appendChild (n, nullptr);
            }
            st.appendChild (edits, nullptr);
        }
    }
    if (auto xml = st.createXml())
        copyXmlToBinary (*xml, dest);
}

void JMCReverseProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    const juce::String path = tree.getProperty ("sourcePath", "").toString();
    std::vector<SibEdit> edits;
    if (auto e = tree.getChildWithName ("SIBEDITS"); e.isValid())
        for (auto n : e)
            edits.push_back ({ (bool) n.getProperty ("add", false), (float) (double) n.getProperty ("start", 0.0),
                               (float) (double) n.getProperty ("end", 0.0) });
    tree.removeChild (tree.getChildWithName ("SIBEDITS"), nullptr);

    apvts.replaceState (tree);
    clearUndoHistory();
    setAutoCapture ((bool) tree.getProperty ("autoCapture", false));

    if (path.isEmpty())
        return;
    const juce::File f (path);
    if (f.existsAsFile())
    {
        const juce::ScopedLock sl (lock);
        pendingSibEdits = edits;
        if (sourceFile == f && source != nullptr) // already loaded: just restore the edits
        {
            sibEdits = pendingSibEdits;
            pendingSibEdits.clear();
            ++sibEditRev;
            return;
        }
    }
    if (f.existsAsFile())
        loadFile (f, false);
    else
    {
        const juce::ScopedLock sl (lock);
        ++generation;
        sourceFile = f;
        source.reset();
        sibFeatures.reset();
        sibEdits = edits;
        errorMessage = "Source file not found: " + f.getFileName() + ". Load it again to continue.";
        state.store (EngineState::Error);
        sourceRevision.fetch_add (1);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JMCReverseProcessor(); }

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <vector>
#include "audio/AudioFileLoader.h"
#include "dsp/RenderSettings.h"
#include "dsp/Sibilance.h"
#include "audio/WavExport.h"
#include <string>

namespace jmc
{
enum class EngineState { Empty, Loading, Analyzing, Rendering, Ready, Error };

// Audio prepared for the audio thread: immutable once published.
struct PlaybackAudio
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
};
} // namespace jmc

class JMCReverseProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    JMCReverseProcessor();
    ~JMCReverseProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "JMC REVERSE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Source loading (message thread) ----
    void loadFile (const juce::File&, bool resetTrim = true);
    juce::File getSourceFile() const;
    jmc::EngineState getState() const { return state.load(); }
    juce::String getErrorMessage() const;
    std::shared_ptr<const jmc::LoadedAudio> getSource() const;
    juce::uint32 getRenderedStats (float& corr, float& monoLossDb, float& width) const { corr = statCorr.load(); monoLossDb = statLoss.load(); width = statWidth.load(); return renderRevision.load(); }
    juce::uint32 getSourceRevision() const { return sourceRevision.load(); }

    // ---- Capture from the track input (insert workflow) ----
    // Records what flows through the plugin (manually, or automatically while the host plays),
    // saves it as a 32-bit float WAV in the captures folder and loads it like any other source.
    void startCapture();
    void stopCapture();                 // finalises: writes the WAV and loads it
    bool isCapturing() const { const int s = capState.load(); return s == 1 || s == 2; }
    double getCaptureSeconds() const { return hostRate.load() > 0 ? capPos.load() / hostRate.load() : 0.0; }
    void setAutoCapture (bool on);
    bool getAutoCapture() const { return autoCapture.load(); }
    void pollCapture();                 // message thread (also driven by an internal timer)
    juce::String getNote() const;       // last user-facing note (e.g. "No audio captured")
    static juce::File capturesDir();

    // ---- Export (message thread) ----
    // The exported file is the same buffer the FX preview plays.
    bool isResultCurrent();                          // result exists and matches the current settings
    juce::String suggestedExportName();              // JMCReverse_<Effect>_<N>bars_<bpm>bpm.wav
    jmc::ExportOptions getExportOptions() const;
    void setExportOptions (const jmc::ExportOptions&);
    void exportResultAsync (const juce::File& dest); // background job; progress via getExportStatus()
    bool exportResultNow (const juce::File& dest, juce::String& error);
    juce::File writeDragFile (juce::String& error);  // temp file for drag & drop into the DAW
    static juce::File dragDir();
    juce::String getExportStatus() const;
    juce::File getLastExportFile() const;
    juce::uint32 getExportRevision() const { return exportRevision.load(); }
    float takePreviewPeak() { return previewPeak.exchange (0.0f); }

    void resetAll();                    // every parameter back to its default, character/preset cleared
    void setLoop (bool l) { loop.store (l); }
    bool isLooping() const { return loop.load(); }

    // ---- Preview transport (any non-audio thread) ----
    void requestRender();                 // any thread; supersedes any render in flight
    void updateRenderIfNeeded();          // message thread, called from the editor timer (debounces edits)
    std::shared_ptr<const jmc::PlaybackAudio> getRendered() const;
    juce::uint32 getRenderRevision() const { return renderRevision.load(); }
    double getRiserSeconds() const /* = onset position: pre-roll + swell */ { return riserSeconds.load(); }
    void setPreviewResult (bool r) { previewMode.store (r ? 1 : 0); }
    bool isPreviewResult() const { return previewMode.load() == 1; }

    void play()  { wantPlay.store (true); }
    void stop()  { wantPlay.store (false); }
    bool isPlaying() const { return wantPlay.load(); }
    void seekNormalised (double pos01);          // resolved on the audio thread against the buffer that is playing
    void playFrom (bool result, double pos01);   // click-to-play: choose RAW (false) / FX (true), jump there and start
    double getPlayheadNormalised() const;

    void collectGarbage();

    // ---- Parameters by id (real units). Used by presets and characters; one undo step. ----
    void applyValues (const std::vector<std::pair<juce::String, float>>& values, const juce::String& presetName = {});
    juce::String getPresetName() const { return apvts.state.getProperty ("preset", "").toString(); }

    // ---- Undo/redo of parameter edits (snapshot based; message thread) ----
    // Host automation and UI edits both create snapshots once the values have been stable for
    // 300 ms (or when undo is pressed), so a drag is one step.
    void commitUndoIfStable();
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void clearUndoHistory();
    void commitUndoNow();          // close the current edit as its own undo step (before presets/characters/reset)

    // ---- Character modes (message thread). 0 = none, 1..12 = jmc::characters ----
    void applyCharacter (int oneBased);
    int getCharacter() const { return (int) apvts.state.getProperty ("character", 0); }
    bool isCharacterModified() const;

    // ---- Sibilance (message thread) ----
    std::vector<jmc::SibRegion> getSibRegions();   // detected + manual edits (fractions of the source)
    juce::String getSibMessage();
    juce::uint32 getSibRevision() const { return sibRevision.load(); }
    void addSibRegion (float start, float end);    // manual override: add
    void removeSibRegionAt (float at);            // manual override: remove whatever covers pos
    void clearSibEdits();

    double getHostBpm() const { return hostBpm.load(); }
    bool hostBpmKnown() const { return hostBpmSeen.load(); }
    int getHostTimeSigNum() const { return tsNum.load(); }
    int getHostTimeSigDen() const { return tsDen.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    jmc::RenderSettings readSettings();
    void ensureSib (float sensitivity);
    void submitRebuild (juce::uint32 gen, std::shared_ptr<const jmc::LoadedAudio> src);
    void publish (juce::uint32 gen, std::shared_ptr<const jmc::LoadedAudio> src, std::shared_ptr<jmc::PlaybackAudio> pb, bool newSource,
                  std::shared_ptr<const jmc::SibFeatures> feats = nullptr);
    void retire (std::shared_ptr<jmc::PlaybackAudio>);

    // Guarded by lock (never touched from the audio thread)
    mutable juce::CriticalSection lock;
    std::shared_ptr<const jmc::LoadedAudio> source;
    juce::String errorMessage;
    juce::uint32 generation = 0;
    std::shared_ptr<jmc::PlaybackAudio> playbackOwner, renderedOwner;
    struct SibEdit { bool add; float start, end; };
    std::shared_ptr<const jmc::SibFeatures> sibFeatures;
    std::vector<SibEdit> sibEdits, pendingSibEdits;
    juce::File sourceFile;
    std::vector<jmc::SibRegion> sibFinal;
    juce::String sibMsg;
    float sibKeySens = -1.0f;
    juce::uint32 sibKeyEdits = 0, sibEditRev = 0, sibFeatRev = 0, sibKeyFeat = 0xffffffff;
    juce::uint32 renderGen = 0;
    struct Snap { std::vector<float> values; int character = 0; juce::String preset; };
    Snap takeSnap() const;
    void restoreSnap (const Snap&);
    bool sameSnap (const Snap&, const Snap&) const;
    std::vector<Snap> undoStack, redoStack;
    Snap lastCommitted, lastSeenSnap;
    juce::uint32 snapChangeMs = 0;
    bool snapInit = false;
    jmc::RenderSettings lastRequested, lastSeen;
    juce::uint32 lastChangeMs = 0;
    struct Retired { std::shared_ptr<jmc::PlaybackAudio> p; juce::uint32 ms; };
    std::vector<Retired> graveyard;

    std::atomic<jmc::EngineState> state { jmc::EngineState::Empty };
    std::atomic<juce::uint32> sourceRevision { 0 };
    std::atomic<double> hostRate { 48000.0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostBpmSeen { false };
    std::atomic<int> tsNum { 4 }, tsDen { 4 };

    // Audio-thread shared
    std::atomic<jmc::PlaybackAudio*> playback { nullptr };   // raw views of the owners above; the
    std::atomic<jmc::PlaybackAudio*> rendered { nullptr };   // graveyard keeps replaced buffers alive
    std::atomic<int> previewMode { 0 };                      // 0 = source (A), 1 = result (B)
    std::atomic<juce::uint32> renderRevision { 0 };
    std::atomic<juce::uint32> sibRevision { 0 };
    std::atomic<double> riserSeconds { 0.0 };
    std::atomic<double> statRiserOnly { 0.0 };
    std::atomic<juce::uint32> exportRevision { 0 };
    std::atomic<float> previewPeak { 0.0f };
    juce::String exportStatus;
    juce::File lastExport;
    std::atomic<float> statCorr { 1.0f }, statLoss { 0.0f }, statWidth { 1.0f };
    std::atomic<bool> wantPlay { false };
    std::atomic<float> seekNorm { -1.0f };
    std::atomic<int> playPos { 0 };
    std::atomic<int> playLength { 0 };

    // Audio-thread private
    jmc::PlaybackAudio* lastPb = nullptr;
    bool active = false;
    float env = 0.0f;
    int pos = 0;

    // Capture (audio thread writes capBuf while capState == 1; see processBlock)
    void timerCallback() override { pollCapture(); }
    void finalizeCapture();
    std::atomic<int> capState { 0 };        // 0 idle, 1 recording, 2 inside a write, 3 stopped/awaiting finalise
    std::atomic<int> capPos { 0 };
    std::atomic<int> capNumCh { 2 };
    std::atomic<bool> capFinalize { false };
    std::atomic<bool> autoCapture { false };
    std::atomic<bool> loop { false };
    std::unique_ptr<juce::AudioBuffer<float>> capBufOwner;
    std::atomic<juce::AudioBuffer<float>*> capBuf { nullptr };
    bool wasPlaying = false;                // audio thread
    juce::String note;

    jmc::RenderCache renderCache; // only touched by the (single) render worker
    juce::ThreadPool pool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JMCReverseProcessor)
};

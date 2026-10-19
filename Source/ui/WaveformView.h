#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>

// Draws real min/max peaks computed from audio data (one lane per channel),
// optional trim handles, source-onset marker and playhead.
class WaveformView : public juce::Component, public juce::SettableTooltipClient
{
public:
    using Buffer = std::shared_ptr<const juce::AudioBuffer<float>>;

    std::function<void (double)> onSeek;   // click / drag: play from this position
    std::function<void()> onStop;          // double-click
    std::function<void()> onDragOut;       // click-and-drag away: hand the audio to the DAW (result view only)
    bool dragOutEnabled = false;
    std::function<void (double start, double end, bool gestureBegin, bool gestureEnd)> onTrim;
    std::function<void (double start, double end)> onAddRegion;   // Shift-drag
    std::function<void (double pos)> onRemoveRegion;              // Alt/Option-click

    void setBuffer (Buffer b) { buffer = std::move (b); peaksWidth = -1; repaint(); }
    void setPlayhead (double p, bool visible) { if (std::abs (p - playhead) > 1e-5 || visible != showPlayhead) { playhead = p; showPlayhead = visible; repaint(); } }
    void setMessage (const juce::String& m) { if (m != message) { message = m; repaint(); } }
    void setDropHighlight (bool on) { if (on != dropHighlight) { dropHighlight = on; repaint(); } }
    void setEmptyText (const juce::String& t) { emptyText = t; }
    void setDisplayGamma (float gm) { gamma = gm; peaksWidth = -1; repaint(); } // < 1 lifts quiet parts (display only)
    void setWaveColour (juce::Colour c) { waveColour = c; repaint(); }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void enableTrim (bool e) { trimEnabled = e; }
    void setTrim (double s, double e) { if (dragging == None) { trimStart = s; trimEnd = e; repaint(); } }
    void setRegions (std::vector<std::pair<double, double>> r) { if (r != regions) { regions = std::move (r); repaint(); } }
    void setMarker (double pos01) { marker = pos01; repaint(); } // < 0 hides

    void paint (juce::Graphics&) override;
    void resized() override { peaksWidth = -1; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (buffer != nullptr && onStop) onStop(); }

private:
    enum Drag { None, Start, End, Seek, AddRegion };
    void rebuildPeaks (int width);

    Buffer buffer;
    std::vector<std::vector<std::pair<float, float>>> peaks; // [channel][pixel]
    int peaksWidth = -1;
    std::vector<std::pair<double, double>> regions;
    double regionAnchor = 0.0, regionNow = 0.0;
    double playhead = 0.0, trimStart = 0.0, trimEnd = 1.0, marker = -1.0;
    bool dropHighlight = false;
    bool showPlayhead = false, trimEnabled = false;
    Drag dragging = None;
    juce::uint32 lastSeekMs = 0;
    int downX = 0;
    bool dragOutStarted = false;
    juce::String message, emptyText, title;
    float gamma = 1.0f;
    juce::Colour waveColour { 0xffaeb3bd };
};

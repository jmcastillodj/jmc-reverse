#include "WaveformView.h"
#include "LookAndFeel.h"

void WaveformView::rebuildPeaks (int width)
{
    peaks.clear();
    peaksWidth = width;
    if (buffer == nullptr || width <= 0)
        return;
    const int n = buffer->getNumSamples();
    for (int c = 0; c < buffer->getNumChannels(); ++c)
    {
        std::vector<std::pair<float, float>> lane ((size_t) width, { 0.0f, 0.0f });
        const float* d = buffer->getReadPointer (c);
        for (int x = 0; x < width; ++x)
        {
            const int a = juce::jmin (n - 1, int (int64_t (x) * n / width));
            const int b = juce::jmax (a + 1, int (int64_t (x + 1) * n / width));
            float lo = d[a], hi = d[a];
            for (int i = a; i < b && i < n; ++i)
            {
                lo = juce::jmin (lo, d[i]);
                hi = juce::jmax (hi, d[i]);
            }
            lane[(size_t) x] = { lo, hi };
        }
        peaks.push_back (std::move (lane));
    }
}

void WaveformView::paint (juce::Graphics& g)
{
    struct DropOverlay
    {
        WaveformView& v; juce::Graphics& g;
        ~DropOverlay()
        {
            if (! v.dropHighlight) return;
            auto r = v.getLocalBounds().toFloat();
            g.setColour (jmc::ui::col::accent.withAlpha (0.14f));
            g.fillRect (r);
            g.setColour (jmc::ui::col::accent);
            g.drawRect (r, 2.0f);
            g.setColour (jmc::ui::col::strong);
            g.setFont (jmc::ui::sans (12.0f, "Bold", 0.14f));
            g.drawText ("DROP AUDIO TO USE IT AS THE SOURCE", v.getLocalBounds(), juce::Justification::centred);
        }
    } dropOverlay { *this, g };

    auto r = getLocalBounds().toFloat();
    g.setColour (jmc::ui::col::panel);
    g.fillRect (r);

    if (buffer == nullptr || buffer->getNumSamples() == 0)
    {
        g.setColour (jmc::ui::col::faint);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (message.isNotEmpty() ? message : emptyText, getLocalBounds(), juce::Justification::centred);
        g.setColour (jmc::ui::col::line);
        g.drawRect (r, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (jmc::ui::col::faint);
            g.setFont (jmc::ui::sans (9.5f, "Bold", 0.14f));
            g.drawText (title, getLocalBounds().reduced (8, 4), juce::Justification::topLeft);
        }
        return;
    }

    const int w = getWidth();
    if (peaksWidth != w)
        rebuildPeaks (w);

    const int lanes = (int) peaks.size();
    const float laneH = r.getHeight() / (float) juce::jmax (1, lanes);
    for (int c = 0; c < lanes; ++c)
    {
        const float cy = r.getY() + laneH * ((float) c + 0.5f);
        g.setColour (jmc::ui::col::lineSoft);
        g.drawHorizontalLine ((int) cy, 0.0f, (float) w);
        g.setColour (waveColour);
        for (int x = 0; x < w; ++x)
        {
            const auto [lo, hi] = peaks[(size_t) c][(size_t) x];
            auto shape = [this] (float v) { v = juce::jlimit (-1.0f, 1.0f, v); return v < 0 ? -std::pow (-v, gamma) : std::pow (v, gamma); };
            const float y1 = cy - shape (hi) * laneH * 0.48f;
            const float y2 = cy - shape (lo) * laneH * 0.48f;
            g.drawVerticalLine (x, y1, juce::jmax (y1 + 1.0f, y2));
        }
    }

    g.setColour (juce::Colour (0x40d0312d));
    for (auto& [ra, rb] : regions)
        g.fillRect ((float) (ra * w), 0.0f, juce::jmax (1.0f, (float) ((rb - ra) * w)), r.getHeight());
    if (dragging == AddRegion)
    {
        g.setColour (juce::Colour (0x66ffffff));
        const double x0 = juce::jmin (regionAnchor, regionNow) * w, x1 = juce::jmax (regionAnchor, regionNow) * w;
        g.fillRect ((float) x0, 0.0f, (float) (x1 - x0), r.getHeight());
    }

    if (marker >= 0.0)
    {
        g.setColour (jmc::ui::col::ice);
        g.drawVerticalLine ((int) (marker * w), 0.0f, r.getHeight());
    }

    if (trimEnabled)
    {
        const float sx = (float) (trimStart * w), ex = (float) (trimEnd * w);
        g.setColour (juce::Colour (0xb0090a0b));
        g.fillRect (0.0f, 0.0f, sx, r.getHeight());
        g.fillRect (ex, 0.0f, (float) w - ex, r.getHeight());
        g.setColour (jmc::ui::col::red);
        g.fillRect (sx - 1.0f, 0.0f, 2.0f, r.getHeight());
        g.fillRect (ex - 1.0f, 0.0f, 2.0f, r.getHeight());
        g.fillRect (sx, 0.0f, 6.0f, 4.0f);
        g.fillRect (ex - 6.0f, 0.0f, 6.0f, 4.0f);
    }

    if (showPlayhead)
    {
        g.setColour (jmc::ui::col::strong);
        g.drawVerticalLine ((int) (playhead * w), 0.0f, r.getHeight());
    }
    if (title.isNotEmpty())
    {
        g.setColour (jmc::ui::col::dim);
        g.setFont (jmc::ui::sans (9.5f, "Bold", 0.14f));
        g.drawText (title, getLocalBounds().reduced (8, 4), juce::Justification::topLeft);
    }
    g.setColour (jmc::ui::col::line);
    g.drawRect (r, 1.0f);
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    if (buffer == nullptr || getWidth() <= 0)
        return;
    const double w = getWidth();
    if (e.mods.isAltDown())
    {
        if (onRemoveRegion) onRemoveRegion (juce::jlimit (0.0, 1.0, double (e.x) / w));
        return;
    }
    if (e.mods.isShiftDown() && onAddRegion)
    {
        dragging = AddRegion;
        regionAnchor = regionNow = juce::jlimit (0.0, 1.0, double (e.x) / w);
        return;
    }
    if (trimEnabled && std::abs (e.x - trimStart * w) <= 7)
        dragging = Start;
    else if (trimEnabled && std::abs (e.x - trimEnd * w) <= 7)
        dragging = End;
    else
    {
        dragging = Seek;
        dragOutStarted = false;
        downX = e.x;
        lastSeekMs = juce::Time::getMillisecondCounter();
        if (onSeek) onSeek (juce::jlimit (0.0, 1.0, double (e.x) / w)); // a click plays from here
        return;
    }
    if (onTrim)
        onTrim (trimStart, trimEnd, true, false);
    mouseDrag (e);
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (buffer == nullptr || getWidth() <= 0 || dragging == None)
        return;
    const double p = juce::jlimit (0.0, 1.0, double (e.x) / getWidth());
    constexpr double minLen = 0.005;
    if (dragging == AddRegion)
    {
        regionNow = p;
        repaint();
        return;
    }
    if (dragging == Seek && dragOutEnabled)
    {
        if (e.getDistanceFromDragStart() > 7 && ! dragOutStarted && onDragOut)
        {
            dragOutStarted = true;
            onDragOut();
        }
        return;
    }
    if (dragging == Seek)
    {
        // Dragging scrubs: re-position at most every 90 ms (the click itself already started playback).
        const auto now = juce::Time::getMillisecondCounter();
        if (onSeek && std::abs (e.x - downX) > 4 && now - lastSeekMs > 90)
        {
            lastSeekMs = now;
            onSeek (p);
        }
        return;
    }
    if (dragging == Start) trimStart = juce::jmin (p, trimEnd - minLen);
    else                   trimEnd   = juce::jmax (p, trimStart + minLen);
    repaint();
    if (onTrim) onTrim (trimStart, trimEnd, false, false);
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (dragging == AddRegion && onAddRegion)
    {
        const double a = juce::jmin (regionAnchor, regionNow), b = juce::jmax (regionAnchor, regionNow);
        if (b - a > 0.002)
            onAddRegion (a, b);
        repaint();
    }
    if ((dragging == Start || dragging == End) && onTrim)
        onTrim (trimStart, trimEnd, false, true);
    dragging = None;
}

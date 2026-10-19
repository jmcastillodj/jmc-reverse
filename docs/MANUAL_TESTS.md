# Manual test plan (Studio One / Fender Studio Pro 8)

Automated tests cannot cover the host. Run these after each milestone; tick or note the result.

## Export and drag & drop (M8)
1. Insert JMC REVERSE on a vocal track. Press **Capture**, play 2-4 s of the vocal, press **Stop**. State goes Analyzing -> Rendering -> Ready.
2. **Export WAV** is greyed out and the drag zone says "Rendering - drag when Ready" until Ready; turn a knob: both grey out again for ~0.2 s then re-enable.
3. Click **Export WAV**: native save dialog opens with `JMCReverse_<Effect>_<N>bars_<bpm>bpm.wav` (check bars and bpm against the project). Save to Desktop. Status line shows "Exported ... (24-bit, stereo, 48.0 kHz)".
4. Drag the file from the desktop into a new track: it must sound the same as clicking the RESULT waveform (compare by ear; also null test: invert one copy, expect silence when export is 32-bit float at the project rate).
5. Repeat with 32-bit float, each sample rate, and Mono. Check file info in Studio One (rate/bit depth/channels).
6. **Drag & Drop into DAW** zone **and the RESULT waveform itself** (click-and-drag away from it): press and drag onto the arrangement. The clip must land where released and play. Note the clip length and compare with the Length setting (Pre-roll shifts the marker: the grey marker position is where the downbeat falls).
7. Drag with the host stopped, playing, and with the plugin window in the back. Note any failure and the exact behaviour.
8. Export when the destination is read-only (e.g. /Library): a readable error is shown, no crash.
9. Reveal button opens Finder at the last export.

## Capture (M7b)
- Manual capture with the host stopped (nothing should be captured -> message), playing, looping.
- Auto-capture: enable, press play on the host, stop; the capture loads automatically. Change tempo, repeat.
- Silent track: note "captured audio is silent".

## State / automation (M7)
- Save the project, close, reopen: parameters, character/preset, trim, source (file in ~/Library/JMC/JMC REVERSE/Captures) and sibilance edits return.
- Automate Length with Tempo sync on while changing project tempo: result re-renders.
- Undo/Redo buttons, Reset.

## Known not covered by automation
Audible quality (sibilance picks, character presets, pitch glide aliasing at +/-12 st), host timing edge cases, window resizing.

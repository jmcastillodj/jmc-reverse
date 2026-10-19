# Known limitations

## Platform and distribution
- **macOS Apple Silicon, VST3 only.** No Intel, Windows, AU or standalone build.
- **Ad-hoc signed, not notarized.** A copy downloaded from the internet is quarantined by Gatekeeper; `install.sh` removes the flag. Public distribution needs an Apple Developer ID signature and notarization.
- **JUCE licence.** JUCE is AGPLv3 or commercial. Selling this plugin closed-source needs a JUCE commercial licence (see `release/LICENSE-NOTICE.txt`).
- Tested in **Studio One / Fender Studio Pro 8** only. Other hosts follow the VST3 standard but have not been tried.
- The build fetches JUCE from GitHub on first configure (pinned to 8.0.15); an offline build needs a pre-populated `build/_deps`.

## Host integration
- A plug-in cannot insert audio events into the arrangement. The result is delivered by **Export WAV** or by **dragging** the drag zone; you place it on the grid (the marker shows the downbeat position).
- The editor is not resizable; opening **Advanced** widens the window by 496 px. Not checked on every display scale.
- **Cmd-Z** is not routed to the plug-in; use the UNDO / REDO buttons. Undo steps are snapshots of all parameters, so host automation moves are recorded as steps too.
- The preview (clicking a waveform) is added on top of the track's own audio; it does not follow the host transport.
- Project recall reloads the source from its file path. If you move or delete the file the plug-in keeps your settings and shows "Source file not found". Captures are stored in `~/Library/JMC/JMC REVERSE/Captures` and are not cleaned up automatically.

## Audio
- Sources: **WAV, AIFF, FLAC, MP3, M4A/AAC, CAF, OGG** (loaded through the system codecs where needed), mono or stereo, up to 10 minutes. A clip dragged from a DAW delivers the whole underlying file, not the trimmed region: use the red trim handles to select the part. Capture: up to 60 s per take, no pre-roll before the transport starts.
- The effect is rendered at the host sample rate; changing the project rate triggers a re-render (and a short delay). Export can convert to 44.1/48/88.2/96/192 kHz with a windowed-sinc resampler.
- Time-stretching uses WSOLA. The reverb tail is noise-like and stretches cleanly; tonal material may show artefacts at extreme ratios. Decay is automatically limited to 0.25x-3x of the length, so the *Decay* knob is not always the effective decay.
- **Pitch movement** reads the swell at a varying rate without an anti-alias filter: at the extremes (+/-12 semitones) the upward direction can show aliasing.
- **Sibilance detection** is a spectral/temporal heuristic, not AI. It was checked with synthetic signals and informally on your vocals. Hi-hats or cymbal swells longer than ~40 ms that are not continuous can still be detected as sibilants; manual add/remove is provided for those cases. Manual edits belong to one source file.
- Wide settings are limited so the mono sum never loses more than 6 dB (by design, "Width limited" is shown).
- Peak protection only scales the result (to -0.1 dBFS); there is no limiter. Quiet material is never boosted by export (Auto Level matches the swell to the source peak).
- The 12 characters and 27 factory presets are starting points chosen by design reasoning and a few listening checks; tune them by ear.

## Not implemented
- Favourite stars in the preset list (the reference product has them).
- Windows/AU, MIDI, sidechain, resizable UI, AAX.

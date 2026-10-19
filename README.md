# JMC REVERSE

Reverse-reverb risers, swells and transitions from vocals and other audio. VST3, macOS Apple Silicon.

Put it on a vocal track as an insert, **capture** a phrase (or load a WAV/AIFF), pick a **character**, set the
**length** (in bars when tempo-synced), listen, then **export a WAV or drag the result onto the arrangement**.

The effect is a real reverse reverb, not a fade or a reversed sample: the source is reversed, run through an 8-line
feedback reverb, reversed back (so the tail builds into the original onset), time-stretched to the exact length,
then shaped by the tone/FX/envelope stages.

## Quick start
1. Install (see `release/INSTALL.md`). Plug-in appears as **JMC REVERSE** in your host.
2. Insert it on the track. Press **CAPTURE**, play the vocal, press **STOP** (or enable *Auto-capture while the host plays* in the `...` menu). Or **drag any audio file or DAW clip onto the SOURCE waveform** (it lights up), or press **LOAD AUDIO**. Formats: WAV, AIFF, FLAC, MP3, M4A/AAC, CAF, OGG.
3. Drag the red trim handles on the SOURCE waveform to choose the phrase. Optional: turn on **SIBILANCE DETECTION** (Shift-drag adds a region, Alt-click removes one).
4. Click a **CHARACTER**, or turn the big **LENGTH** knob. With **TEMPO SYNC** it snaps to beats/bars of the host tempo.
5. **Click the SOURCE waveform** to hear the original from that point, or **click the RESULT waveform** to hear the generated effect (exactly what is exported). Double-click stops; **LOOP** repeats.
6. **EXPORT WAV** (24-bit / 32-bit float, project rate or 44.1-192 kHz, optional mono), or drag the finished audio into the arrangement: either **click-and-drag the RESULT waveform** itself or use the **DRAG & DROP INTO DAW** area. File name: `JMCReverse_<Effect>_<N>bars_<bpm>bpm.wav`.
   Place the clip so the **ice-blue marker** on the RESULT waveform lands on the downbeat (the plug-in cannot insert clips by itself).

## Documents
- `release/INSTALL.md` - installation and uninstall
- `docs/PARAMETERS.md` - all 43 automatable parameters (generated from the plugin)
- `docs/TESTED.md` - what has been verified, by whom and how
- `docs/LIMITATIONS.md` - known limitations
- `docs/MANUAL_TESTS.md` - host test plan
- `docs/DESIGN.md` - technical design and the verified reference evidence
- `docs/STATUS.md` - milestone log

## Building from source
Requirements: macOS 12+, Apple Silicon, Xcode Command Line Tools, CMake >= 3.22, git, internet access on the first
configure (JUCE 8.0.15 is fetched at a pinned tag).

```bash
./scripts/build.sh Release           # build into ./build
./scripts/build.sh Release install   # ... and install to ~/Library/Audio/Plug-Ins/VST3
./scripts/package.sh                 # clean release build + verification + tests + dist/*.zip
```

Tests (all built by the same CMake project):

| Target | What it covers |
|---|---|
| `build/JMCTests` | DSP engine, sibilance, effects, characters, WAV export, file loading |
| `build/JMCPluginTests` | the real processor: state recall, undo, presets, capture, export, audio-thread safety |
| `build/jmc_hostcheck "<bundle>" [audio.wav]` | loads the finished `.vst3` through a VST3 host and stresses it |
| `build/jmc_bench` | render and analysis timings |
| `build/jmc_snapshot out.png [audio.wav] [adv] [char=N]` | renders the editor off-screen to a PNG |
| `build/jmc_render in.wav out.wav [length] [decay] [dry] [char=N] [bpm=N]` | offline renderer for listening without a host |
| `build/jmc_params` | prints the parameter table used by `docs/PARAMETERS.md` |

## Source layout
`Source/dsp` engine (reverb, stretch, effects, sibilance, renderer) - `Source/audio` file loading, resampler, WAV export -
`Source/ui` look-and-feel, knob, waveform - `PluginProcessor` state/threads - `PluginEditor` UI - `Presets`, `FactoryPresets`,
`Characters` - `Tests` - `scripts` - `release`.

## Licence note
Built on JUCE (AGPLv3 or commercial licence). Commercial closed-source distribution requires a JUCE commercial licence.
See `release/LICENSE-NOTICE.txt`.

## Windows
A 64-bit Windows build (VST3) is produced by GitHub Actions (`.github/workflows/windows.yml`): it builds with the runner's Visual Studio (MSVC, x64),
runs the same test suites, loads the finished plug-in through a VST3 host and uploads `JMC REVERSE <version> Windows x64.zip`.
Installation: `release/windows/INSTALL.md`.

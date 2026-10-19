# JMC REVERSE — Technical Design (v0.1)

## 1. Verified reference functionality

Sources actually read: the AtmosRiser product page (waveatmos.com/products/atmosriser).
The YouTube demo could **not** be read (only page chrome was retrievable: no transcript, description or chapters). Nothing below is attributed to the video. Please supply a transcript or notes if you want video-specific behaviour matched.

Verified from the product page:
- Reverse-reverb plugin producing risers/transitions "in one click".
- Formats: VST3 and AU; macOS and Windows.
- Drag-and-drop of the finished audio file into the session.
- "Automatic Sibilance Detection" (listed; no detail given).
- Built-in one-click effects that change riser texture (not named).
- Factory presets plus user presets (save/load).
- Demos made in Studio One / FL Studio.

Everything else in this spec (durations, tempo sync, 12 characters, etc.) is **proposed JMC REVERSE functionality**, not verified reference behaviour.

### 1b. Additional verified evidence (your screenshots + 6.4 s screen capture, Studio One/Fender Studio Pro)
Verified from the images/video (no audio was analysed, so nothing about *sound*):
- Plugin runs as an **insert on the vocal track**; a riser waveform appears in the plugin while the vocal clip plays => the product **captures the audio flowing through the insert** (no file load needed in the demo).
- Large central knob labelled `REVERSE <n BAR>`: riser length in **bars** (tempo-synced).
- Toggles `PHONE`, `CHORUS`, `TREMOLO`; a popup next to a toggle shows `MIX 100%` and `TIMING 1/8` sliders (tremolo timing division + mix).
- `LISTEN` and `LOOP` buttons; `RAW | FX` switch; `RESET`; hamburger (settings) menu.
- Waveform with **two red trim markers**; shows the riser shape (swell).
- Preset bar: name box with `<` `>` and a **save** button; popup list with `Factory Presets` / `User Presets` groups and **favourite stars**.
- A **Drag & Drop into DAW** drop-zone; the video shows the file being dragged into the arrangement and landing on the grid, named like `AtmosRiser_FX_1bars_100bpm`. **This proves Studio One accepts an external file drag from a plugin window** (Milestone 8 can rely on it, to be tested here).
- Same sidebar in the page: AU + VST3.

Impact on JMC REVERSE (proposed, not yet built): (1) **Capture from track input** as a second source mode besides file loading; (2) PHONE / CHORUS / TREMOLO one-click FX toggles with Mix + Timing; (3) RAW/FX audition switch (we have A/B), LISTEN/LOOP, RESET, favourites; (4) export filename `JMCReverse_<effect>_<N>bars_<bpm>bpm.wav`; (5) bars-based length display. Visual style stays our own (graphite/red), only the *layout logic* follows the reference.

## 2. JMC REVERSE feature list
As in the master spec §3–§11, delivered by milestone. Status of each is tracked in `docs/STATUS.md` and only marked done once tested.

## 3. DSP architecture
Offline render pipeline (background thread, never in the audio callback):

`trim → pre-filter (HP/LP) → [sibilance region select] → resample to project rate → algorithmic reverb (own FDN, 8 lines, Hadamard mixing, modulated delays, damping) with tail padding → reverse → time-stretch to target length (phase-vocoder / WSOLA, or pitch-coupled resample for PITCH RISE/FALL) → envelope (fade in/out, curves) → creative (mod, gate/stutter, saturation) → stereo width (M/S, mono-safe) → mix dry/wet → peak-safe gain (static scale, no limiter) → Rendered buffer`.

Playback: the audio thread reads an immutable `shared_ptr<const RenderedBuffer>` swapped via a lock-free atomic exchange (render jobs carry a generation counter; stale results are dropped). Preview is host-transport-independent (button-triggered) with optional host-sync start.
Export uses the **same buffer** as preview, so output matches.

Modules: `Source/dsp` (reverb, stretch, filters, sibilance, renderer), `Source/audio` (file IO, resampler, wav writer), `Source/state` (APVTS, presets), `Source/ui`, `Tests/`.

## 4. Framework & dependencies
- JUCE 8.0.15 via CMake FetchContent (pinned). C++20, CMake, Unix Makefiles (Ninja not installed; Xcode.app not installed, only Command Line Tools — sufficient for VST3).
- **Licence warning:** JUCE is AGPLv3 or commercial. Commercial closed-source sale of JMC REVERSE needs a JUCE commercial licence (or releasing under AGPL). Your decision.
- VST3 SDK is bundled with JUCE (GPLv3/Steinberg licence terms apply similarly).
- No other runtime deps. Tests: JUCE UnitTest runner (console app target).

## 5. Export & drag-and-drop
- EXPORT WAV: native `FileChooser`, 24-bit int or 32-bit float, SRC to chosen rate, scale-only peak protection.
- Drag-out: `DragAndDropContainer::performExternalDragDropOfFiles` with a temp WAV. Works in many hosts; **Studio One acceptance of external drags from a plugin window is a known risk** and will be tested empirically in Studio Pro 8 (installed). If it fails, the Export button + "reveal in Finder" is the supported workflow; no faking.

## 6. UI layout
Header (logo, preset, load/save, settings) → main display (source + processed waveforms, trim handles, sibilant regions, playhead) → control row (Length, Reverb, Tone, Width, Mix, Sibilance) → character buttons (12) → footer (Export, drag area, peak meter, status). Graphite + restrained red. Delivered in Milestone 9; plain controls before.

## 7. Risks & tests
| Risk | Test |
|---|---|
| Audio-thread safety of buffer swap | TSan build + stress test of repeated renders during playback |
| Stale renders overwrite newer settings | Generation counter unit test |
| Studio One drag-out | Manual test plan in Studio Pro 8 |
| Reverse-reverb quality (clicks, pre-ringing) | Offline renders analysed for discontinuities/peaks; listening |
| Time-stretch artefacts | Null/spectral tests + listening |
| Sibilance false positives (hats, breaths) | Synthetic + real material test set (needs your vocal samples) |
| Tempo-change re-render | Unit test of beat→seconds + host mock |
| State recall | APVTS round-trip test; Studio Pro project reload |
| No automatic listening by me | I can verify numerically and via pluginval-like loading; **subjective listening must be done by you**. |

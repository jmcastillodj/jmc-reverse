# What has been tested

## 1. Automated (re-run with `./scripts/package.sh`, or the targets below)
| Suite | Checks | Covers |
|---|---|---|
| `JMCTests` | 162 | resampler accuracy; WAV/AIFF loading (16/24/32-bit, mono/stereo, 1-sample, corrupt, cancel); reverse-reverb engine: silence, 1-sample, quiet (1e-6), full-scale, 44.1/48/88.2/96/192 kHz, extreme reverb, lengths 0.5-16 s, swell growth >12 dB, edge smoothness, join continuity, determinism, trim, cancellation, render cache identity; WSOLA pitch/level; tempo maths and tempo-change lengths; pre-roll/tail; sibilance (S and SH-like noise between vowels at 44.1/48/96 kHz, vowels only, breath, closed hi-hat, continuous noise, silence, tiny file, hard-panned and out-of-phase stereo, sensitivity monotonic); tilt, pitch glide, stutter, saturation, tempo-ramp tremolo (3 note values), tremolo, curves, width/mono safeguard, balance, modulation, LF damping; all 12 characters render, finite, peak-safe, exact length and mutually different; WAV export (32-bit bit-exact, 24-bit within 1 LSB, mono, 5 rates, scale-only peak protection, failure handling) and file naming |
| `JMCPluginTests` | 67 | the real processor: parameter-ID contract (43 IDs frozen), load -> analyse -> render, project-state round trip (parameters, character, trim, sync, sibilance edits, source path, export options), corrupt state, missing source file, undo/redo, 27 factory presets apply exactly, user preset save/load/delete and sanitising, burst of render requests ends with the newest settings, capture (bit-accurate, pass-through untouched, preview never recorded, silent/short, auto-capture follows transport, 60 s buffer-full), loop, reset, FX toggles, export equals preview, stale result refused, drag file, 600 audio blocks during renders/loads/seeks/A-B |
| `jmc_hostcheck` | 7 | the finished `.vst3` bundle loaded through a VST3 host: scan, 5 sample rates x 5 block sizes pass-through, mono layout, random automation + state recall + editor open/close on another thread, 40 create/load/render/destroy cycles closed mid-work |
| Static checks (`package.sh`) | 5 | arm64 only, valid signature, only system libraries, only the 3 VST3 entry points exported, no development paths in the binary |

Sanitizers: `JMCTests`, `JMCPluginTests` and `jmc_hostcheck` pass under **AddressSanitizer + UndefinedBehaviorSanitizer** with zero reports; `JMCPluginTests` (state, capture, export, 600 audio blocks during renders/loads) passes under **ThreadSanitizer** with zero reports. Finding fixed on the way: the bundle exported ~7,800 JUCE symbols (JMC VERB and AirLimit export 3), which could collide with other JUCE plug-ins in the same DAW process; it now exports only the 3 VST3 entry points.

Performance (`jmc_bench`, Apple Silicon, Release): 10 s source -> 4 s swell at 48 kHz renders in ~0.11 s; 16 s swell with 12 s decay ~0.35 s; 30 s source at 96 kHz ~0.34 s; worst case (10 s source, 30 s swell, 192 kHz, decay 20 s) ~1.0 s. Changing only downstream settings (tone, FX, envelope, width, levels, dry, tail) reuses the cached reverb: ~9 ms instead of ~120 ms. Sibilance analysis of 60 s stereo: 0.1 s at 48 kHz, 0.5 s at 192 kHz; sensitivity changes cost ~0.01 ms.

## 2. Verified by you in Studio One / Fender Studio Pro 8 (as you reported at each milestone)
Loads and instantiates; audio loading, waveform and preview; reverse-reverb renders sound as intended; tempo sync follows the project BPM; sibilance detection picks the right sounds on your vocals; the 12 characters and effects work; capture from the track input works; WAV export and dragging into the arrangement work; the final interface looks and works well.

## 3. Implemented and covered by automation but NOT explicitly confirmed in the host
Project save/reopen recall, automation lanes, undo/redo buttons, preset save/load in the UI, auto-capture on transport, Advanced drawer resize, Export while the host tempo changes. Use `docs/MANUAL_TESTS.md`.

## 4. Not possible to automate
Subjective sound quality and musicality of presets/characters; listening for artefacts; behaviour in hosts other than Studio Pro 8.

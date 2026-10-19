# Installing JMC REVERSE

Requirements: a Mac with Apple Silicon (M1 or newer), macOS 12 or later, a VST3 host (tested in Studio One / Fender Studio Pro 8).

## Easy way
1. Unzip the download.
2. Open Terminal in the unzipped folder and run `./install.sh` (or right-click it > Open).
3. Restart your DAW or rescan plug-ins. The plugin appears as **JMC REVERSE** (Fx > Reverb).

## By hand
Copy `JMC REVERSE.vst3` to `~/Library/Audio/Plug-Ins/VST3/` (system-wide: `/Library/Audio/Plug-Ins/VST3/`).
If macOS says the plugin cannot be opened, run:
`xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/JMC REVERSE.vst3"`
The plugin is ad-hoc signed (not notarized), so downloaded copies need that command once.

## Where it keeps its files
- Presets: `~/Library/JMC/JMC REVERSE/Presets`
- Audio captured from a track: `~/Library/JMC/JMC REVERSE/Captures` (projects refer to these files)
- Temporary drag files: the system temp folder (deleted after 2 hours)

## Uninstall
`./uninstall.sh` (add `--purge` to also delete presets and captures).

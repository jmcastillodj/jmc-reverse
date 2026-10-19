# Installing JMC REVERSE on Windows

Requirements: Windows 10 or 11, 64-bit (x64), a VST3 host (Studio One, Cubase, Reaper, FL Studio, Ableton Live, Bitwig, ...).
No extra runtime is needed (the Visual C++ runtime is built in).

## Easy way
1. Unzip the download (right-click > Extract All).
2. Right-click `install.bat` > **Run as administrator**. It copies the plug-in to `C:\Program Files\Common Files\VST3`.
3. Restart your DAW or rescan plug-ins. **JMC REVERSE** appears under Fx / Reverb.

## By hand
Copy the whole folder `JMC REVERSE.vst3` into `C:\Program Files\Common Files\VST3\` (all users) or, if your DAW scans it,
`%LOCALAPPDATA%\Programs\Common\VST3\` (just you).

## If Windows warns about the file
The plug-in is not code-signed. If SmartScreen or your antivirus complains, right-click the zip > Properties > tick
**Unblock** before extracting, or choose "More info > Run anyway" for `install.bat`.

## Where it keeps its files
- Presets and audio captured from a track: `%APPDATA%\JMC\JMC REVERSE\` (`Presets`, `Captures`)
- Temporary files for drag & drop: your Windows temp folder (removed after 2 hours)

## Uninstall
Run `uninstall.bat` (add `/purge` to also delete presets and captures).

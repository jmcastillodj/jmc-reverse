#!/bin/bash
# Installs JMC REVERSE for the current user (macOS, Apple Silicon).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$DEST"
rm -rf "$DEST/JMC REVERSE.vst3"
cp -R "$HERE/JMC REVERSE.vst3" "$DEST/"
# A downloaded copy carries the quarantine flag; the plugin is ad-hoc signed, not notarized.
xattr -dr com.apple.quarantine "$DEST/JMC REVERSE.vst3" 2>/dev/null || true
echo "Installed to $DEST/JMC REVERSE.vst3"
echo "Rescan plug-ins in your DAW (Studio One / Fender Studio Pro: Preferences > Locations > VST Plug-ins > Reset Blocklist / Rescan)."

#!/bin/bash
# Removes JMC REVERSE. Your presets and captures are kept unless you pass --purge.
set -euo pipefail
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/JMC REVERSE.vst3"
echo "Removed the plugin."
if [ "${1:-}" = "--purge" ]; then
  rm -rf "$HOME/Library/JMC/JMC REVERSE"
  echo "Removed user presets and captures."
else
  echo "Kept: ~/Library/JMC/JMC REVERSE (presets, captures). Use --purge to delete them."
fi

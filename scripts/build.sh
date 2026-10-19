#!/bin/bash
# Reproducible build: ./scripts/build.sh [Release|Debug]  (add "install" to copy to ~/Library)
set -euo pipefail
cd "$(dirname "$0")/.."
CFG="${1:-Release}"
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="$CFG"
cmake --build build --config "$CFG" -j"$(sysctl -n hw.ncpu)"
codesign --force --deep -s - build/JMCReverse_artefacts/"$CFG"/VST3/"JMC REVERSE.vst3"
if [ "${2:-}" = "install" ]; then
  mkdir -p ~/Library/Audio/Plug-Ins/VST3
  rm -rf ~/Library/Audio/Plug-Ins/VST3/"JMC REVERSE.vst3"
  cp -R build/JMCReverse_artefacts/"$CFG"/VST3/"JMC REVERSE.vst3" ~/Library/Audio/Plug-Ins/VST3/
fi

#!/bin/bash
# Clean Release build -> verification -> tests -> distributable zip in dist/.
# Usage: ./scripts/package.sh
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=$(grep -m1 -E 'project\(JMCReverse VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')
NAME="JMC REVERSE $VERSION macOS arm64"
BUILD=build-release
OUT="dist/$NAME"

rm -rf "$BUILD" dist
cmake -S . -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)" 2>&1 | tee "$BUILD/build.log" | grep -E "error|warning:" | grep -v -E "jobserver|splash" || true
test -d "$BUILD/JMCReverse_artefacts/Release/VST3/JMC REVERSE.vst3"
BUNDLE="$BUILD/JMCReverse_artefacts/Release/VST3/JMC REVERSE.vst3"
BIN="$BUNDLE/Contents/MacOS/JMC REVERSE"
codesign --force --deep -s - "$BUNDLE"

echo "== verification"
fail() { echo "VERIFY FAILED: $1"; exit 1; }
[ "$(lipo -archs "$BIN")" = "arm64" ] || fail "binary is not arm64-only"
codesign --verify --deep --strict "$BUNDLE" || fail "signature invalid"
BAD=$(otool -L "$BIN" | tail -n +2 | awk '{print $1}' | grep -v -E '^(/System/Library/|/usr/lib/)' || true)
[ -z "$BAD" ] || fail "links against non-system libraries: $BAD"
[ "$(nm -gU "$BIN" | wc -l | tr -d ' ')" = "3" ] || fail "bundle exports more than the 3 VST3 entry points"
if strings "$BIN" | grep -q -E '/Users/|/private/tmp|Documents/02'; then fail "hard-coded development path found in the binary"; fi
plutil -lint "$BUNDLE/Contents/Info.plist" >/dev/null || fail "Info.plist invalid"
echo "arm64 only, signed, system libraries only, 3 exported symbols, no dev paths"

echo "== tests"
"$BUILD/JMCTests" | tail -1
"$BUILD/JMCPluginTests" | tail -1
"$BUILD/jmc_hostcheck" "$BUNDLE" | tail -1

echo "== staging"
mkdir -p "$OUT/docs"
cp -R "$BUNDLE" "$OUT/"
cp release/install.sh release/uninstall.sh release/INSTALL.md "$OUT/"
cp README.md "$OUT/"
cp docs/PARAMETERS.md docs/TESTED.md docs/LIMITATIONS.md docs/MANUAL_TESTS.md "$OUT/docs/"
cp release/LICENSE-NOTICE.txt "$OUT/"
(cd dist && ditto -c -k --keepParent "$NAME" "$NAME.zip" && shasum -a 256 "$NAME.zip" | tee "$NAME.zip.sha256")
echo "Release: dist/$NAME.zip"

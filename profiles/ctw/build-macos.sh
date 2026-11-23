#!/usr/bin/env bash
# Native macOS build of this profile (no Wine, no emulator). Twin of the Windows scripts build-tools.cmd,
# translate.cmd and build-native.cmd, run through tools/pspport/posix_profile.py.
#   ./build-macos.sh              decrypt, translate and compile (needs game_files/ with your own disc files)
#   JOBS=4 ./build-macos.sh       parallel compile jobs (default 2: the generated code is memory hungry)
#   ./build-macos.sh compile      only compile again, after a first full run
# Packages: Xcode Command Line Tools (xcode-select --install), then: brew install cmake ninja pkg-config sdl2 ffmpeg
# Rendering: Direct3D 12 exists only on Windows; here the software renderer draws and SDL2 shows the frame.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
step="${1:-all}"
exec "${PYTHON:-python3}" "$here/../../tools/pspport/posix_profile.py" "$step" --port "$here" --jobs "${JOBS:-2}"

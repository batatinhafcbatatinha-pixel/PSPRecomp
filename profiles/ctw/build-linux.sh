#!/usr/bin/env bash
# Native linux build of this profile (no Wine, no emulator). Twin of the Windows scripts build-tools.cmd,
# translate.cmd and build-native.cmd, run through tools/pspport/posix_profile.py.
#   ./build-linux.sh              decrypt, translate and compile (needs game_files/ with your own disc files)
#   JOBS=4 ./build-linux.sh       parallel compile jobs (default 2: the generated code is memory hungry)
#   ./build-linux.sh compile      only compile again, after a first full run
# Packages (Debian/Ubuntu): clang pkg-config python3 libsdl2-dev libavcodec-dev libavformat-dev
#   libavutil-dev libswscale-dev libswresample-dev   (cmake and ninja: the launcher installs them, or use your own)
# Rendering: Direct3D 12 exists only on Windows; here the software renderer draws and SDL2 shows the frame.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
step="${1:-all}"
exec "${PYTHON:-python3}" "$here/../../tools/pspport/posix_profile.py" "$step" --port "$here" --jobs "${JOBS:-2}"

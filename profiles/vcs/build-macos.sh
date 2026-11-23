#!/usr/bin/env bash
# Native macOS build of this profile (no Wine, no emulator). Twin of BUILD_VCS.bat.
# Needs profiles/vcs/generated/, translated locally from your own executable (see README.md); it is not distributed.
#   ./build-macos.sh              configure (once) and build into build/native
#   JOBS=4 ./build-macos.sh       parallel jobs (default 2: the generated code is memory hungry)
#   ./build-macos.sh -DNAME=VALUE extra CMake options on the first configure
# Packages: Xcode Command Line Tools, then: brew install cmake ninja pkg-config sdl2 ffmpeg
# Rendering: Direct3D 12 exists only on Windows; here the software renderer draws and SDL2 shows the frame.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
build="${BUILD_DIR:-$here/build/native}"
if [ ! -f "$here/generated/generated_registry.cpp" ]; then
  echo "No generated code in $here/generated: translate your own executable first (see README.md)." >&2
  exit 1
fi
if [ ! -f "$build/build.ninja" ]; then
  launcher=()
  command -v ccache >/dev/null && launcher=(-DCMAKE_CXX_COMPILER_LAUNCHER=ccache) || true
  cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE=vcs     -DPSPRECOMP_BUILD_TESTS=OFF -DPSPRECOMP_BUILD_PROFILE_TESTS=OFF "${launcher[@]}" "$@"
fi
cmake --build "$build" --target VCSNative --parallel "${JOBS:-2}"
echo "Built: $build/bin/Release/VCSNative"

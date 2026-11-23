#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
build="${BUILD_DIR:-$root/build/posix}"
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DPSPRECOMP_PROFILE= -DPSPRECOMP_BUILD_NATIVE_HOST=ON "$@"
cmake --build "$build" --parallel "${JOBS:-2}"

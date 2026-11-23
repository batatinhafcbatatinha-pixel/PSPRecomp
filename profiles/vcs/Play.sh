#!/usr/bin/env bash
# Starts this profile on Linux / macOS (built with build-linux.sh or build-macos.sh). Twin of PLAY_VCS.bat.
#   ./Play.sh [game_root]   game_root holds PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF (default: ./game)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
bin="${BUILD_DIR:-$here/build/native}/bin/Release/VCSNative"
[ -x "$bin" ] || { echo "Build first: ./build-linux.sh (or ./build-macos.sh)" >&2; exit 3; }
game="${1:-$here/game}"
elf="$game/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF"
[ -f "$elf" ] || { echo "Missing $elf: pass the game root as the first argument." >&2; exit 4; }
export PSPRECOMP_CONFIG="${PSPRECOMP_CONFIG:-$here/config/VCSNative.ini}"
cd "$(dirname "$bin")"
exec "$bin" "$elf" "$game"

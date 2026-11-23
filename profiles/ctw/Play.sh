#!/usr/bin/env bash
# Starts this profile on Linux / macOS (built with build-linux.sh or build-macos.sh). Twin of Play-CTW.cmd.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
exec "${PYTHON:-python3}" "$here/../../tools/pspport/posix_profile.py" play --port "$here" "$@"

# Linux and macOS

Native build and play, no Wine and no emulator. **Untested**: this was written without running anything on Linux or
macOS. Expect compile errors and runtime bugs; please report them.

## What is supported

- The framework and tools (`psp_recomp`, `psp_analyze`, `dump_function`) and the tests: plain CMake, no extra libraries.
- The profile in `profiles/ctw` and discs without a profile (`profiles/generic`, `tools/pspport`): decrypt, translate, compile and
  play from your own disc files. The launcher uses the `linux` / `macos` blocks of `launcher.json` and `profiles/ctw/setup.json`.
- The profile in `profiles/vcs` builds with `profiles/vcs/build-linux.sh` / `build-macos.sh` once you have translated your own
  executable into `profiles/vcs/generated` (see its README). It is not installed by the launcher.

## How it differs from Windows

- Rendering is the software rasterizer. The Direct3D 12 renderer, the Vulkan option and the HDR shaders are Windows only
  (the non-Windows stubs in the host report "software").
- The window, keyboard/pad input and audio come from SDL2 (`tools/native/`), replacing the Win32 window and audio files.
  Window and sound can be switched off with `PSPRECOMP_WINDOW=0` and `PSPRECOMP_AUDIO=0`.
- The game thread runs on a 64 MiB stack thread; the main thread keeps the window (required on macOS).
- FFmpeg comes from the system (pkg-config; versioned FFmpeg 7 sonames as fallback), not from the bundled headers.
- Keys: arrows = D-pad, W A S D = stick, I/J/K/L = triangle/square/cross/circle, Q/E = shoulders, Enter = Start,
  Backspace = Select, Esc = quit. A game controller works too.
- The in-game settings page of the launcher is not offered (its options are for the Windows renderers).

## Build

Packages: a C++20 compiler (clang or g++ 11+), CMake 3.20+, Ninja, pkg-config, SDL2 and FFmpeg development files.

- Debian/Ubuntu: `sudo apt install clang ninja-build cmake pkg-config libsdl2-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev`
- macOS: `xcode-select --install`, then `brew install cmake ninja pkg-config sdl2 ffmpeg`

```
bash tools/build-posix.sh            # tools, tests and the host libraries, no game content
profiles/ctw/build-linux.sh          # or build-macos.sh; needs game_files/ with your disc
profiles/ctw/Play.sh
```

CMake option `PSPRECOMP_BUILD_NATIVE_HOST=ON` (Linux/macOS only, default OFF) compiles the host sources of the profiles
without any generated code; CI uses it.

## Known not to work

- Direct3D 12 / Vulkan renderers, `SimulateHDR`, the GPU backend options, DXGI card choice.
- Windows-only profile scripts (`.cmd`, `.bat`) and the `scripts/` helpers that download Windows FFmpeg builds.
- No game install has been tried through the launcher on either system; macOS Intel and Apple silicon are both unchecked.
- Speed is unknown: the software renderer is much slower than the Direct3D 12 path.

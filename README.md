# PSPRecomp (fork with game ports)

Fork of [jessicanataliagta/PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp) with game profiles in
[`profiles/`](profiles/) and a launcher that builds and starts them from **your own disc**. No game content (disc image,
executable, data or generated code) is in this repository.

## How to play a game

Windows 10/11 x64. About 15 GB free and 30-90 minutes for the first build.

1. Get this repository, either way:
   - `git clone --recurse-submodules https://github.com/batatinhafcbatatinha-pixel/PSPRecomp.git`, or
   - **Code > Download ZIP** and unpack it.
2. Run **`Launcher.cmd`** (it downloads `Launcher.exe` from the
   [port-installer releases](https://github.com/batatinhafcbatatinha-pixel/port-installer/releases) the first time).
3. Click **CHOOSE ISO...** and pick your UMD image (`.iso`). The game is recognised from the disc.
4. Confirm **Install**. The launcher installs what is missing (Python, Visual Studio C++ Build Tools, CMake, Ninja),
   copies and checks the game files, decrypts your `EBOOT.BIN` on your PC (it downloads and compiles
   [pspdecrypt](https://github.com/John-K/pspdecrypt) for that; nothing of it is in this repository), translates the game
   code, fetches FFmpeg 7.1 and compiles. Finished steps are skipped next time. Log: **Install** tab and
   `profiles/<game>/setup-log.txt`.
5. **PLAY**.

| Game | Disc | State |
| --- | --- | --- |
| Grand Theft Auto: Chinatown Wars ([`profiles/ctw`](profiles/ctw/)) | PSP, USA (ULUS-10490) | not playable yet: boots its threads, black screen |

Only this exact disc is supported (checked by SHA-256, see `profiles/ctw/installer.json`).

## Differences from upstream

- `profiles/ctw`: GTA: Chinatown Wars profile (host, HLE, DX12 renderer, config), setup steps and play script
- launcher + one-click install/build/play (`Launcher.cmd`, [port-installer](https://github.com/batatinhafcbatatinha-pixel/port-installer) submodule in `tools/port-installer`)
- analysis seeds for ELFs without relocations: unrelocated code pointers and jump tables
- codegen fixes: JAL to import stubs, sparse dispatch locals
- `addi` decode, semaphore and event-flag timeouts
- guest memory dump on runtime exceptions; overlay ELF helper (`profiles/ctw/tools/make_overlay_elf.py`)

## Building

Windows: Visual Studio 2022 (or Build Tools) with the C++ workload, CMake 3.20+, Ninja, Python 3.11+.

Framework and tools:

```bat
git clone --recurse-submodules https://github.com/batatinhafcbatatinha-pixel/PSPRecomp.git
cd PSPRecomp
cmake -S . -B out/framework -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE=""
cmake --build out/framework
```

One game, without the launcher (`-Source` is your disc image):

```bat
powershell -ExecutionPolicy Bypass -File tools\port-installer\setup\setup.ps1 -GameDir profiles\ctw -ToolsDir .local\tools -Source "D:\ISO\your-umd.iso"
```

Or step by step in `profiles/ctw/` (from an "x64 Native Tools" prompt, disc files in `game_files/`): `build-tools.cmd`,
`python scripts/decrypt_eboot.py`, `translate.cmd`, `python scripts/fetch_ffmpeg.py`, `build-native.cmd`, then
`Play-CTW.cmd`.

## Upstream documentation

PSPRecomp is a static recompilation framework for PSP software. It reads an Allegrex/MIPS executable, analyzes guest code, emits C++ translation units, and runs them through a native host runtime instead of shipping a PSP interpreter or JIT.

The repository is split between a reusable framework and game-specific profiles. The first working profile is GTA: Vice City Stories (`profiles/vcs`).

### Repository layout

```text
include/psprecomp/   Public runtime and Allegrex interfaces
src/                 ELF/PRX loading, decoder, memory, runtime and support code
tools/               Generic analyzer, recompiler and reverse-engineering helpers
tests/               Framework regression tests
configs/             Generic examples and PSP NID data
profiles/            Game-specific hosts, generated code, configuration and tests
  vcs/               GTA: Vice City Stories profile
```

Game-specific addresses, HLE behavior, native fast paths, renderer integration and generated AOT code belong under a profile. The framework should remain usable without any profile selected.

### Requirements

- CMake 3.20 or newer
- A C++20 compiler
- Visual Studio 2022 for the current Windows/DX12 VCS build

### Build the framework only

```bash
cmake -S . -B out/framework -DPSPRECOMP_PROFILE=""
cmake --build out/framework --config Release
ctest --test-dir out/framework -C Release --output-on-failure
```

This builds `psprecomp_core`, `psp_analyze`, `psp_recomp`, `dump_function` and the framework tests.

### Build a profile

Profiles are selected with `PSPRECOMP_PROFILE`:

```bash
cmake -S . -B out/vcs -DPSPRECOMP_PROFILE=vcs
cmake --build out/vcs --config Release
```

Windows users working on the VCS profile can use the maintained scripts in `profiles/vcs/scripts`.

### Create another profile

See [`docs/PROFILE_GUIDE.md`](docs/PROFILE_GUIDE.md). A new title normally provides its own generated corpus, HLE/profile host, configuration, tests and optional native fast paths without modifying the framework for game-specific addresses.

### Game files

No commercial game executable or asset is included. PSPRecomp does not ship an EBOOT decryption implementation. Profiles expect files obtained from the user's own copy in the format documented by that profile.

### Source provenance

See [`docs/SOURCE_PROVENANCE.md`](docs/SOURCE_PROVENANCE.md) for the project rules around independently written code, profile boundaries, decryption and third-party source.

### Third-party code

The framework is MIT licensed. Individual profiles may include separately licensed dependencies or assets; their notices stay beside those files. The VCS profile lists its bundled dependencies in `profiles/vcs/THIRD_PARTY.md`.

### License

PSPRecomp framework code is distributed under the MIT License. See [`LICENSE`](LICENSE).

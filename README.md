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

## Screenshots

Screenshots of each port at the furthest point it reaches are being captured and will appear here, one framed
picture per game with its name.

## Upstream

This repository is a fork of [jessicanataliagta/PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp); its documentation (pipeline, runtime, tools) is
in that project's README and in `docs/`. The original license and credits apply unchanged.

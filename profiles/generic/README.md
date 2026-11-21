# Generic profile (any PSP game)

The launcher's path for a PSP disc that has no profile of its own: the same steps for every game, no game-specific
code. `tools/pspport/pspport.py` drives it; the launcher creates the port folder (`.local/ports/<disc id>`, e.g.
`.local/ports/ULUS-10041`) from your disc and runs:

1. **prepare**: builds `psp_recomp` once for the repository (`.local/pspport/fw`), decrypts your
   `PSP_GAME/SYSDIR/EBOOT.BIN` on your PC (pspdecrypt, downloaded and compiled there; an EBOOT that is already a plain
   ELF is copied), translates it with `psp_recomp --auto` into `build/gen` and fetches FFmpeg 7.1.
2. **compile**: `PSPNative.exe` from this profile: the PSP host shared with the ctw profile (HLE kernel and system
   libraries, GE renderer on DirectX 12, audio, input; its address-specific hooks are not installed) linked with the
   translated code.
3. **play**: `PSPNative.exe build/EBOOT.dec game_files`. Settings: `PSPNative.ini` beside the executable (written
   once, your changes are kept).

By hand, from the repository root, with CMake, Ninja and llvm-mingw (or MSVC) in `PATH`:

```bat
python tools\pspport\pspport.py prepare --port .local\ports\ULUS-10041
python tools\pspport\pspport.py compile --port .local\ports\ULUS-10041
python tools\pspport\pspport.py play --port .local\ports\ULUS-10041
```

Known limits: games that keep code in overlays loaded from their own files (as Chinatown Wars does with
`OVERLAYS.PAK`) need that code translated too, which the generic path does not do yet; widescreen and the right-stick
camera are written for specific games and are off.

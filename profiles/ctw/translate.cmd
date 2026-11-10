@echo off
rem Translates your decrypted executable (build\EBOOT.dec) into C++ in build\gen.
setlocal
set "RECOMP=%~dp0build\fw\psp_recomp.exe"
if not exist "%RECOMP%" for /r "%~dp0build\fw" %%f in (psp_recomp.exe) do if exist "%%f" set "RECOMP=%%f"
rem EBOOT functions reached only from the code overlays (written by tools\make_ctw_overlays.py).
if exist "%~dp0build\overlays\eboot_seeds.txt" set "PSPRECOMP_EXTRA_SEEDS=%~dp0build\overlays\eboot_seeds.txt"
"%RECOMP%" "%~dp0build\EBOOT.dec" --auto "%~dp0build\gen" 0x08804000 0x8000
exit /b %errorlevel%

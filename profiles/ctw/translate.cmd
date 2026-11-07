@echo off
rem Translates your decrypted executable (build\EBOOT.dec) into C++ in build\gen.
setlocal
set "RECOMP=%~dp0build\fw\psp_recomp.exe"
if not exist "%RECOMP%" for /r "%~dp0build\fw" %%f in (psp_recomp.exe) do if exist "%%f" set "RECOMP=%%f"
"%RECOMP%" "%~dp0build\EBOOT.dec" --auto "%~dp0build\gen" 0x08804000 0x8000
exit /b %errorlevel%

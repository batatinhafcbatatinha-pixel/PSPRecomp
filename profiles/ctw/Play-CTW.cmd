@echo off
rem Starts GTA: Chinatown Wars (built by the launcher: Launcher.cmd at the repository root).
setlocal
set "BIN=%~dp0build\native\bin"
if not exist "%BIN%\CTWNative.exe" (
  echo The game is not built yet: use Launcher.cmd at the repository root.
  pause
  exit /b 1
)
cd /d "%BIN%"
"%BIN%\CTWNative.exe" "%~dp0build\EBOOT.dec" "%~dp0game_files"
exit /b %errorlevel%

@echo off
rem Compiles CTWNative.exe (profile ctw + the generated code in build\gen) into build\native\bin.
rem build-native.cmd [--jobs N]
setlocal
set "VSLANG=1033"
set "JOBS=4"
if /i "%~1"=="--jobs" set "JOBS=%~2"
rem llvm-mingw (setup sets CC/CXX to clang) needs no Visual Studio environment.
if not defined CC where cl >nul 2>nul || for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
set "GEN=%~dp0build\gen"
set "GEN=%GEN:\=/%"
rem Code overlays (build\ovlgen, from tools\regen_overlays.py) are compiled in when present.
set "OVL=%~dp0build\ovlgen"
set "OVL=%OVL:\=/%"
if not exist "%~dp0build\ovlgen\ovl_registry.cpp" set "OVL="
cmake -S "%~dp0..\.." -B "%~dp0build\native" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE=ctw "-DCTW_GENERATED_DIR=%GEN%" "-DCTW_OVERLAY_GENERATED_DIR=%OVL%" -DPSPRECOMP_BUILD_TESTS=OFF || exit /b 1
cmake --build "%~dp0build\native" --target CTWNative --parallel %JOBS%
exit /b %errorlevel%

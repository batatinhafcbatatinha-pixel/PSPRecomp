@echo off
rem Builds the game-neutral PSPRecomp tools (psp_recomp, psp_analyze) into build\fw.
setlocal
set "VSLANG=1033"
rem llvm-mingw (setup sets CC/CXX to clang) needs no Visual Studio environment.
if not defined CC where cl >nul 2>nul || for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake -S "%~dp0..\.." -B "%~dp0build\fw" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE= -DPSPRECOMP_BUILD_TESTS=OFF || exit /b 1
cmake --build "%~dp0build\fw" --target psp_recomp psp_analyze
exit /b %errorlevel%

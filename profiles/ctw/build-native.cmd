@echo off
rem Compiles CTWNative.exe (profile ctw + the generated code in build\gen) into build\native\bin.
rem build-native.cmd [--jobs N]
setlocal
set "VSLANG=1033"
set "JOBS=4"
if /i "%~1"=="--jobs" set "JOBS=%~2"
where cl >nul 2>nul || for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
set "GEN=%~dp0build\gen"
set "GEN=%GEN:\=/%"
cmake -S "%~dp0..\.." -B "%~dp0build\native" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE=ctw "-DCTW_GENERATED_DIR=%GEN%" -DPSPRECOMP_BUILD_TESTS=OFF || exit /b 1
cmake --build "%~dp0build\native" --target CTWNative --parallel %JOBS%
exit /b %errorlevel%

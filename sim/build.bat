@echo off
REM Configures and builds the desktop simulator.
REM
REM Uses full paths to cmake.exe/ninja.exe rather than prepending them to
REM PATH: a chained one-liner like
REM   call vcvars64.bat && set "PATH=...;%PATH%" && cmake ...
REM silently breaks, because %PATH% in the `set` command is substituted at
REM cmd.exe's *parse* time (before vcvars64.bat has run), so it captures the
REM *pre*-vcvars PATH and overwrites the good one vcvars64.bat just set -
REM cl.exe then can't be found even though `where cl` right after vcvars64.bat
REM alone finds it fine. A .bat file's lines don't have this problem since
REM each is expanded when it actually runs, but this avoids the whole class
REM of bug entirely.
setlocal

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA_EXE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "VCPKG_TOOLCHAIN=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\vcpkg\scripts\buildsystems\vcpkg.cmake"

call "%VCVARS%"
if errorlevel 1 exit /b 1

where cl >nul
if errorlevel 1 (
    echo cl.exe not found on PATH after vcvars64.bat - is Visual Studio Build Tools installed?
    exit /b 1
)

cd /d "%~dp0"

"%CMAKE_EXE%" -S . -B build -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
if errorlevel 1 exit /b 1

"%CMAKE_EXE%" --build build
if errorlevel 1 exit /b 1

echo.
echo Build OK: build\c5_lvgl_ui_sim.exe

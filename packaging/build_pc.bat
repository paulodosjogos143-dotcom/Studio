@echo off
REM ============================================================================
REM  ROXBOX - Build both applications for PC (Windows .exe)
REM  Requires: CMake ^>= 3.16 and a C++17 compiler (Visual Studio Build Tools
REM  or MinGW). Run this file; it configures, compiles and copies the .exe
REM  files into the dist\ folder ready to run.
REM ============================================================================
setlocal
cd /d "%~dp0"

echo.
echo ============================================================
echo   ROXBOX PC Builder - producing ROXBOX Studio + Player .exe
echo ============================================================
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake not found in PATH.
    echo         Install CMake from https://cmake.org and re-run this script.
    exit /b 1
)

echo [1/3] Configuring (desktop OpenGL 3.3 core build)...
cmake -S . -B build_pc -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

echo.
echo [2/3] Compiling...
cmake --build build_pc --config Release -j
if errorlevel 1 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

echo.
echo [3/3] Collecting .exe files into dist\ ...
if not exist dist mkdir dist
copy /Y "build_pc\bin\Release\ROXBOX_Studio.exe"   "dist\ROXBOX_Studio.exe" >nul
copy /Y "build_pc\bin\Release\ROXBOX_Player.exe"   "dist\ROXBOX_Player.exe" >nul
if not exist "dist\saves" mkdir dist\saves

echo.
echo ============================================================
echo   DONE. Your executables are in:
echo       %CD%\dist\ROXBOX_Studio.exe
echo       %CD%\dist\ROXBOX_Player.exe
echo.
echo   Double-click to run. Studio opens the editor; Player runs the game.
echo ============================================================
endlocal

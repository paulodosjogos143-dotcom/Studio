@echo off
REM ============================================================================
REM  ROXBOX - ONE-CLICK INSTALLER / BUILDER
REM  Builds and compiles BOTH applications on your PC automatically:
REM    - ROXBOX Studio (editor)  -> .exe
REM    - ROXBOX Player   (game)   -> .exe
REM  (and, if the Android SDK is present, also builds the .apk)
REM
REM  Just double-click this file. It checks for the tools it needs, builds
REM  everything, and drops the finished programs in the dist\ folder.
REM ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo.
echo #############################################################
echo #   ROXBOX One-Click Installer / Builder                    #
echo #   Builds Studio (.exe) + Player (.exe) [+ .apk if SDK]    #
echo #############################################################
echo.

REM -- 1. Detect the C++ toolchain -------------------------------------------
set "HAVE_CMAKE=0"
where cmake >nul 2>nul && set "HAVE_CMAKE=1"

if "!HAVE_CMAKE!"=="0" (
    echo [CHECK] CMake not found on PATH.
    echo.
    echo   ROXBOX needs CMake ^>= 3.16 and a C++17 compiler to build.
    echo   The easiest way to get both:
    echo     1. Install "Visual Studio Build Tools 2022" (includes the compiler)
    echo     2. Install CMake from https://cmake.org/download
    echo   Then re-run this installer.
    echo.
    echo   Alternatively, run the prebuilt .exe files from dist\ if present.
    echo.
    if exist "dist\ROXBOX_Studio.exe" (
        echo   Prebuilt executables already exist in dist\ - launching Player...
        start "" "dist\ROXBOX_Player.exe"
    )
    exit /b 1
)

echo [CHECK] CMake found.

REM -- 2. Build the PC executables -------------------------------------------
echo.
echo [BUILD] Compiling ROXBOX Studio and Player for PC...
call "%~dp0packaging\build_pc.bat"
if errorlevel 1 (
    echo.
    echo [ERROR] PC build failed. See the messages above.
    exit /b 1
)

echo.
echo [OK] PC build complete.

REM -- 3. Try the Android .apk if the SDK is available ------------------------
set "HAVE_ANDROID=0"
if defined ANDROID_HOME set "HAVE_ANDROID=1"
if defined ANDROID_SDK_ROOT set "HAVE_ANDROID=1"

if "!HAVE_ANDROID!"=="1" (
    echo.
    echo [BUILD] ANDROID_HOME detected - also building the Player .apk...
    call "%~dp0packaging\build_android.sh" || echo [WARN] Android build skipped or failed (optional).
) else (
    echo.
    echo [SKIP] Android SDK not detected - skipping .apk build.
    echo        To build the .apk, install the Android SDK + NDK and set
    echo        ANDROID_HOME, then re-run this installer.
    echo        The PC .exe build is already complete and ready to use.
)

echo.
echo #############################################################
echo #   ALL DONE.                                             #
echo #############################################################
echo.
echo   Your programs are ready in the dist\ folder:
if exist "dist\ROXBOX_Studio.exe" echo     - dist\ROXBOX_Studio.exe   (editor)
if exist "dist\ROXBOX_Player.exe"  echo     - dist\ROXBOX_Player.exe   (game)
if exist "dist\ROXBOX_Player.apk"  echo     - dist\ROXBOX_Player.apk   (Android)
echo.
echo   Double-click an .exe to run it. Install the .apk via:
echo       adb install -r dist\ROXBOX_Player.apk
echo.
endlocal

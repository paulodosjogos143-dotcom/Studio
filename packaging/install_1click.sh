#!/usr/bin/env bash
# ============================================================================
#  ROXBOX - ONE-CLICK INSTALLER / BUILDER (Linux / macOS / Windows-with-bash)
#  Builds and compiles BOTH applications automatically:
#    - ROXBOX Studio (editor)  -> binary
#    - ROXBOX Player   (game)   -> binary
#  (and, if the Android SDK is present, also builds the .apk)
#
#  Run:  bash install_1click.sh
# ============================================================================
set -uo pipefail
cd "$(dirname "$0")"

echo
echo "#############################################################"
echo "#   ROXBOX One-Click Installer / Builder"
echo "#   Builds Studio + Player [+ .apk if SDK]"
echo "#############################################################"
echo

if ! command -v cmake >/dev/null 2>&1; then
    echo "[CHECK] CMake not found."
    echo
    echo "  ROXBOX needs CMake >= 3.16 and a C++17 compiler."
    echo "  Install:  sudo apt install cmake g++   (Debian/Ubuntu)"
    echo "            sudo pacman -S cmake gcc     (Arch)"
    echo "            brew install cmake           (macOS)"
    echo "  Then re-run this installer."
    exit 1
fi

echo "[CHECK] CMake found."
echo
echo "[BUILD] Compiling ROXBOX Studio and Player..."
bash packaging/build_pc.sh
PC_RC=$?
if [ $PC_RC -ne 0 ]; then
    echo "[ERROR] PC build failed. See messages above."
    exit 1
fi
echo "[OK] PC build complete."

if [ -n "${ANDROID_HOME:-}${ANDROID_SDK_ROOT:-}" ]; then
    echo
    echo "[BUILD] Android SDK detected - also building the .apk..."
    bash packaging/build_android.sh || echo "[WARN] Android build skipped/failed (optional)."
else
    echo
    echo "[SKIP] Android SDK not detected - skipping .apk build."
    echo "       Install the Android SDK + NDK, set ANDROID_HOME, re-run."
fi

echo
echo "#############################################################"
echo "#   ALL DONE."
echo "#############################################################"
echo "  Binaries ready in dist/:"
ls -1 dist/ 2>/dev/null | sed 's/^/    - dist\//'
echo
echo "  Run:  ./dist/ROXBOX_Studio   or   ./dist/ROXBOX_Player"
echo "  APK:  adb install -r dist/ROXBOX_Player.apk"
echo

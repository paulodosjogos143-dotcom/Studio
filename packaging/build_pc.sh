#!/usr/bin/env bash
# ============================================================================
#  ROXBOX - Build both applications for PC (Linux / macOS)
#  Requires: CMake >= 3.16, a C++17 compiler, and OpenGL dev headers
#  (Mesa/X11 on Linux). Produces ROXBOX_Studio + ROXBOX_Player binaries in
#  dist/. On Windows use build_pc.bat instead.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"

echo
echo "============================================================"
echo "  ROXBOX PC Builder - producing ROXBOX Studio + Player"
echo "============================================================"
echo

if ! command -v cmake >/dev/null 2>&1; then
    echo "[ERROR] CMake not found in PATH."
    echo "        Install CMake from https://cmake.org and re-run."
    exit 1
fi

echo "[1/3] Configuring (desktop OpenGL build)..."
cmake -S . -B build_pc -DCMAKE_BUILD_TYPE=Release

echo
echo "[2/3] Compiling..."
cmake --build build_pc --config Release -j

echo
echo "[3/3] Collecting binaries into dist/ ..."
mkdir -p dist/saves
cp -f build_pc/bin/ROXBOX_Studio "dist/ROXBOX_Studio"
cp -f build_pc/bin/ROXBOX_Player "dist/ROXBOX_Player"
chmod +x dist/ROXBOX_Studio dist/ROXBOX_Player

echo
echo "============================================================"
echo "  DONE. Your binaries are in:"
echo "      $(pwd)/dist/ROXBOX_Studio"
echo "      $(pwd)/dist/ROXBOX_Player"
echo
echo "  Run: ./dist/ROXBOX_Studio   or   ./dist/ROXBOX_Player"
echo "============================================================"

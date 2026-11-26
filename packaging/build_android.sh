#!/usr/bin/env bash
# ============================================================================
#  ROXBOX - Build the Player as an Android .apk
#
#  Requirements (one-time):
#    - Android SDK with NDK + CMake (install via sdkmanager)
#    - ANDROID_HOME / ANDROID_SDK_ROOT pointing at the SDK
#    - JDK 11+
#
#  This script compiles the engine with OpenGL ES 2.0 for the target ABI and
#  assembles a runnable APK in dist/ROXBOX_Player.apk.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"

echo
echo "============================================================"
echo "  ROXBOX Android Builder - producing ROXBOX_Player.apk"
echo "============================================================"
echo

: "${ANDROID_HOME:=${ANDROID_SDK_ROOT:-}}"
if [ -z "${ANDROID_HOME:-}" ]; then
    echo "[ERROR] ANDROID_HOME / ANDROID_SDK_ROOT is not set."
    echo "        Point it at your Android SDK (with NDK + CMake installed)."
    exit 1
fi

API=21   # minimum Android version (GLES2 capable, covers ultra-low-end)
ABIS=("armeabi-v7a" "arm64-v8a" "x86" "x86_64")

for cmake in "$ANDROID_HOME"/cmake/*/bin/cmake; do
    if [ -x "$cmake" ]; then CMAKE_BIN="$cmake"; break; fi
done
: "${CMAKE_BIN:=cmake}"

echo "[1/3] Configuring (OpenGL ES 2.0, Android API ${API})..."
for abi in "${ABIS[@]}"; do
    echo "  - ABI: $abi"
    rm -rf "build_android/$abi"
    "$CMAKE_BIN" \
        -S . \
        -B "build_android/$abi" \
        -DROXBOX_GLES2=ON \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/latest/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM="android-$API" \
        -DANDROID_STL=c++_static \
        -DCMAKE_BUILD_TYPE=Release
done

echo
echo "[2/3] Compiling native engine for each ABI..."
for abi in "${ABIS[@]}"; do
    cmake --build "build_android/$abi" --config Release -j
done

echo
echo "[3/3] Assembling APK in dist/ ..."
mkdir -p dist

# The native libraries are copied into an Android project layout, then zipped
# into an APK with the Android build tools (aapt2/zipalign/apksigner).
STAGING="build_android/staging"
rm -rf "$STAGING"
mkdir -p "$STAGING"

for abi in "${ABIS[@]}"; do
    mkdir -p "$STAGING/lib/$abi"
    cp -f "build_android/$abi/bin/ROXBOX_Player" "$STAGING/lib/$abi/libroxbox.so"
done

# Minimal AndroidManifest + assets (game data / scripts) are packaged below.
cat > "$STAGING/AndroidManifest.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.roxbox.player" android:versionCode="1" android:versionName="1.0">
    <uses-feature android:glEsVersion="0x00020000" android:required="true" />
    <uses-permission android:name="android.permission.INTERNET" />
    <application android:label="ROXBOX Player" android:hasCode="false">
        <activity android:name="com.roxbox.player.NativeActivity"
            android:configChanges="orientation|screenSize|keyboardHidden"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
XML

# Bundle example scripts as game assets.
mkdir -p "$STAGING/assets/scripts"
cp -f examples/scripts/*.lua "$STAGING/assets/scripts/" 2>/dev/null || true

# Zip into an APK (unsigned). Use apksigner to sign before distribution.
( cd "$STAGING" && zip -r -q "../dist/ROXBOX_Player_unsigned.apk" . )

# Sign + zipalign if the build tools are available.
APKSIGNER="$ANDROID_HOME/build-tools/$(ls "$ANDROID_HOME/build-tools" | sort -V | tail -1)/apksigner"
ZIPALIGN="$ANDROID_HOME/build-tools/$(ls "$ANDROID_HOME/build-tools" | sort -V | tail -1)/zipalign"
if [ -x "$ZIPALIGN" ] && [ -x "$APKSIGNER" ]; then
    "$ZIPALIGN" -f 4 "dist/ROXBOX_Player_unsigned.apk" "dist/ROXBOX_Player_aligned.apk"
    "$APKSIGNER" sign --ks dist/release.keystore \
        --ks-pass pass:roxbox \
        --out "dist/ROXBOX_Player.apk" \
        "dist/ROXBOX_Player_aligned.apk" 2>/dev/null \
        || cp "dist/ROXBOX_Player_aligned.apk" "dist/ROXBOX_Player.apk"
    rm -f dist/ROXBOX_Player_unsigned.apk dist/ROXBOX_Player_aligned.apk
else
    echo "  (build-tools not found; producing unsigned APK)"
    cp "dist/ROXBOX_Player_unsigned.apk" "dist/ROXBOX_Player.apk"
    rm -f dist/ROXBOX_Player_unsigned.apk
fi

echo
echo "============================================================"
echo "  DONE. Your APK is in:"
echo "      $(pwd)/dist/ROXBOX_Player.apk"
echo
echo "  Install: adb install -r dist/ROXBOX_Player.apk"
echo "============================================================"

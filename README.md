# ROXBOX — Roblox Studio (Editor) + Player (Game) Clone

A complete, from-scratch clone of the **Roblox Studio editor** and **Roblox
Player game runtime**, built as a single C++17 engine with a Lua scripting
layer that mirrors the Roblox API (`workspace`, `Instance.new`, `Part`,
`task.wait`, `spawn`, `RunService.RenderStepped`, `Vector3`, `Color3`, …).

The engine is **heavily optimized** to hold **60 FPS on ultra-weak hardware
(~0.015 TFLOPS / 15 GFLOPS / ~30k AnTuTu)** using **OpenGL ES 2.0** and
**Greedy Meshing**, while **automatically scaling graphics up to maximum
quality on powerful PCs**.

---

## What you get

| Component | Description |
|-----------|-------------|
| **ROXBOX Studio** | The editor: voxel viewport, build/paint/erase/move tools, properties panel, script editor, save/load `.roxmap`. |
| **ROXBOX Player** | The game: third-person character, voxel AABB collision, HUD, mobile touch controls, runs place scripts. |
| **Engine** | Voxel world, Greedy Meshing, OpenGL ES 2.0 / desktop GL renderer, adaptive quality manager, Lua host + Roblox API. |

## Key engine features

- **OpenGL ES 2.0** rendering path (GLSL ES 1.00) for Android / low-end, plus a
  desktop OpenGL 3.3 core path for PC.
- **Greedy Meshing** — faces of adjacent opaque voxels are merged into single
  quads (a solid cube = 6 quads = 24 verts), slashing draw calls and RAM.
- **Adaptive quality manager** with 4 tiers, from *Minimum (15 GFLOPS)* to
  *Maximum*. In auto mode it monitors FPS and steps the tier up/down, scaling
  render distance, AO, resolution, meshing budget, fog and particles.
- **Low RAM**: chunk-based streaming, texture atlas, minimal per-chunk meshes.
- **Lua scripting** with a Roblox-compatible API and a protected coroutine
  scheduler (`task.wait`/`spawn`) driven by the engine frame loop.

## Build & run

### One-click installer (easiest)
- **Windows:** double-click `packaging/install_1click.bat`
- **Linux/macOS:** `bash packaging/install_1click.sh`

It checks for the toolchain, compiles **both** apps, and places the finished
programs in the `dist/` folder. If the Android SDK is present it also builds
the `.apk`.

### Manual PC build
```bash
# Windows
packaging\build_pc.bat

# Linux/macOS
bash packaging/build_pc.sh
```
Output: `dist/ROXBOX_Studio` (`.exe` on Windows) and `dist/ROXBOX_Player`.

### Android build
```bash
ANDROID_HOME=/path/to/sdk bash packaging/build_android.sh
```
Output: `dist/ROXBOX_Player.apk`. Install with `adb install -r dist/ROXBOX_Player.apk`.

### Requirements
- **PC:** CMake ≥ 3.16, a C++17 compiler (Visual Studio Build Tools or GCC/Clang), OpenGL.
- **Android:** Android SDK with NDK + CMake, JDK 11+.

## Verification

A headless smoke test exercises the whole pipeline (world gen, greedy meshing,
map save/load, the full Roblox API, the task.wait scheduler, RenderStepped
events, quality auto-scaling). Build and run it without a GPU:

```bash
make            # builds smoke_test, studio_headless, player_headless
./smoke_test    # ALL TESTS PASSED
```

## Project layout

```
engine/
  core/       log, math, time, fs
  world/      voxel world + default place generation
  mesh/       greedy mesher
  render/     camera, texture atlas, quality manager, GL backend, renderer
  scripting/  Lua host + Roblox API (robloxapi)
  ui/         immediate-mode UI
  platform/   win32 / linux / android / null (headless) windows
studio/       editor app
player/       game runtime app
tests/        smoke_test
examples/     sample place scripts
packaging/    build scripts + one-click installer
```

## Running

```bash
./dist/ROXBOX_Studio              # open the editor
./dist/ROXBOX_Player              # play the default world
./dist/ROXBOX_Player --auto       # auto quality scaling
./dist/ROXBOX_Player --tier 0     # force minimum quality (15 GFLOPS target)
./dist/ROXBOX_Player --mobile     # touch controls
./dist/ROXBOX_Player path/to/map.roxmap   # load a saved world
```

# ROXBOX - Roblox Studio/Player clone engine
#
# Builds the engine as a static library and the three applications:
#   - ROXBOX Studio   (editor)
#   - ROXBOX Player   (game runtime)
#   - smoke_test      (headless verification)
#
# Three rendering configurations are supported via the ROXBOX_* options:
#   -DROXBOX_HEADLESS=ON   stub GL, no driver needed (CI / smoke tests)
#   -DROXBOX_GLES2=ON      OpenGL ES 2.0  (Android + ultra-low-end PC, GLSL ES 1.00)
#   (default)              Desktop OpenGL 3.3 core (PC, GLSL 330)
#
# Examples:
#   cmake -S . -B build -DROXBOX_GLES2=ON      # Android / low-end
#   cmake -S . -B build                        # PC desktop
#   cmake -S . -B build -DROXBOX_HEADLESS=ON  # headless verification
#   cmake --build build -j

cmake_minimum_required(VERSION 3.16)
project(ROXBOX CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# ---------------------------------------------------------------------------
# Rendering configuration
# ---------------------------------------------------------------------------
set(ROXBOX_HEADLESS OFF CACHE BOOL "Use stub GL (no driver) for verification")
set(ROXBOX_GLES2   OFF CACHE BOOL "Use OpenGL ES 2.0 (Android / ultra-low-end)")

if(ROXBOX_HEADLESS)
    set(ROXBOX_DEFINES "ROXBOX_HEADLESS")
elseif(ROXBOX_GLES2)
    set(ROXBOX_DEFINES "ROXBOX_GLES2")
else()
    set(ROXBOX_DEFINES "") # desktop GL3
endif()
message(STATUS "ROXBOX render config: ${ROXBOX_DEFINES}")

# ---------------------------------------------------------------------------
# Engine source set
# ---------------------------------------------------------------------------
set(ROXBOX_ENGINE_SOURCES
    engine/core/log.cpp
    engine/core/math.cpp
    engine/core/time.cpp
    engine/core/fs.cpp
    engine/world/voxelworld.cpp
    engine/mesh/mesher.cpp
    engine/render/camera.cpp
    engine/render/textureatlas.cpp
    engine/render/quality.cpp
    engine/render/gl_backend.cpp
    engine/render/renderer.cpp
    engine/scripting/luahost.cpp
    engine/scripting/robloxapi.cpp
    engine/ui/font.cpp
    engine/ui/ui.cpp
    engine/platform/null_window.cpp
    engine/engine.cpp
)

# Platform window: pick the implementation matching the target.
# null_window is always present; the real window files guard themselves by
# platform macros, so they compile to nothing on mismatched targets.
if(ROXBOX_HEADLESS)
    list(FILTER ROXBOX_ENGINE_SOURCES EXCLUDE REGEX "platform/(win32|linux|android)_window")
elseif(ANDROID)
    list(FILTER ROXBOX_ENGINE_SOURCES EXCLUDE REGEX "platform/(win32|linux|null)_window")
    list(APPEND ROXBOX_ENGINE_SOURCES engine/platform/android_window.cpp)
elseif(WIN32)
    list(FILTER ROXBOX_ENGINE_SOURCES EXCLUDE REGEX "platform/(win32|linux|null)_window")
    list(APPEND ROXBOX_ENGINE_SOURCES engine/platform/win32_window.cpp)
elseif(UNIX)
    list(FILTER ROXBOX_ENGINE_SOURCES EXCLUDE REGEX "platform/(win32|linux|null)_window")
    list(APPEND ROXBOX_ENGINE_SOURCES engine/platform/linux_window.cpp)
endif()

# Lua (amalgamated-style single set of C files)
file(GLOB ROXBOX_LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/*.c)
list(FILTER ROXBOX_LUA_SOURCES EXCLUDE REGEX "onelua\\.c$")

add_library(roxbox_engine STATIC ${ROXBOX_ENGINE_SOURCES} ${ROXBOX_LUA_SOURCES})
target_include_directories(roxbox_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/render
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/world
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua
)
target_compile_definitions(roxbox_engine PUBLIC ${ROXBOX_DEFINES})

# Link the real GL/GLES driver for non-headless builds.
if(NOT ROXBOX_HEADLESS)
    if(ROXBOX_GLES2)
        if(ANDROID)
            target_link_libraries(roxbox_engine PUBLIC GLESv2 EGL android log)
        else()
            target_link_libraries(roxbox_engine PUBLIC GLESv2 EGL)
        endif()
    elseif(WIN32)
        target_link_libraries(roxbox_engine PUBLIC opengl32 gdi32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(roxbox_engine PUBLIC OpenGL::GL)
        if(UNIX)
            target_link_libraries(roxbox_engine PUBLIC X11)
        endif()
    endif()
endif()

if(WIN32)
    target_link_libraries(roxbox_engine PUBLIC winmm)
endif()

# ---------------------------------------------------------------------------
# Applications
# ---------------------------------------------------------------------------
add_executable(ROXBOX_Studio studio/main.cpp studio/studio.cpp)
target_link_libraries(ROXBOX_Studio PRIVATE roxbox_engine)

add_executable(ROXBOX_Player player/main.cpp)
target_link_libraries(ROXBOX_Player PRIVATE roxbox_engine)

# Headless verification build
add_executable(smoke_test tests/smoke_test.cpp)
target_link_libraries(smoke_test PRIVATE roxbox_engine)

# Output binaries into a predictable folder.
set_target_properties(ROXBOX_Studio ROXBOX_Player smoke_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

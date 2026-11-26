// ROXBOX - engine core: world, rendering, scripting, quality, main loop
#pragma once
#include "core/log.h"
#include "core/time.h"
#include "world/voxelworld.h"
#include "mesh/mesher.h"
#include "render/gl_backend.h"
#include "render/renderer.h"
#include "render/quality.h"
#include "render/camera.h"
#include "scripting/luahost.h"
#include "scripting/robloxapi.h"
#include "ui/ui.h"
#include "platform/window.h"
#include "input/input.h"
#include <string>

namespace rb {

enum class AppMode { Player, Studio };

struct EngineConfig {
    AppMode mode = AppMode::Player;
    WindowSpec window;
    std::string mapPath;      // empty -> procedural default world
    bool generateDefault = true;
    bool headless = false;
    int startTier = 3;        // initial quality tier
    bool autoQuality = true;
    // if set, the engine takes ownership of this window instead of creating one
    PlatformWindow* externalWindow = nullptr;
};

class Engine {
public:
    virtual bool init(const EngineConfig& cfg);
    void run(); // blocks until window closes / requestQuit
    void requestQuit() { m_quit = true; }

    // app hooks
    virtual void onUpdate(float dt) {}
    virtual void onRenderUI(UI& ui, const InputState& in) {}
    virtual void onQuit() {}

    VoxelWorld& world() { return m_world; }
    Renderer& renderer() { return m_renderer; }
    GLBackend& backend() { return m_backend; }
    RobloxApi& api() { return m_api; }
    LuaHost& lua() { return m_lua; }
    QualityManager& quality() { return m_quality; }
    Camera& camera() { return m_camera; }
    PlatformWindow* window() { return m_window; }
    InputState& input() { return m_input; }
    UI& ui() { return m_ui; }
    float timeSec() const { return m_timeSec; }
    fpsCounter& fps() { return m_fps; }
    bool ready() const { return m_ready; }

protected:
    void frame(float dt);

private:
    EngineConfig m_cfg;
    VoxelWorld m_world;
    GLBackend m_backend;
    Renderer m_renderer{m_backend};
    LuaHost m_lua;
    RobloxApi m_api{m_lua};
    QualityManager m_quality;
    Camera m_camera;
    UI m_ui{m_backend};
    PlatformWindow* m_window = nullptr;

    InputState m_input;
    fpsCounter m_fps;
    bool m_ready = false;
    bool m_quit = false;
    float m_timeSec = 0.f;
};

} // namespace rb

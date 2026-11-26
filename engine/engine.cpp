#include "engine.h"

namespace rb {

bool Engine::init(const EngineConfig& cfg) {
    m_cfg = cfg;
    m_quality.mode = cfg.autoQuality ? QualityManager::Mode::Auto : QualityManager::Mode::Manual;
    if (!cfg.autoQuality) m_quality.setManual(cfg.startTier);
    else m_quality.tier = cfg.startTier;

    if (cfg.externalWindow) {
        m_window = cfg.externalWindow;
    } else {
        m_window = createPlatformWindow();
    }
    if (!m_window->create(cfg.window)) {
        RB_LOG_E("failed to create window");
        return false;
    }

    if (!m_backend.init(m_window->width(), m_window->height())) {
        RB_LOG_E("backend init failed");
        return false;
    }
    if (!m_renderer.init(m_backend, m_window->width(), m_window->height())) {
        RB_LOG_E("renderer init failed");
        return false;
    }
    if (!m_ui.init(m_backend, m_window->width(), m_window->height())) {
        RB_LOG_E("UI init failed");
        return false;
    }

    m_api.init(m_lua);

    // world: load map or generate default
    if (!cfg.mapPath.empty()) {
        if (!m_world.loadFromFile(cfg.mapPath)) {
            RB_LOG_W("map load failed, generating default world");
            generateDefaultWorld(m_world);
        }
    } else if (cfg.generateDefault) {
        generateDefaultWorld(m_world);
    }

    // initial camera framing
    m_camera.target = m_world.spawn;
    m_camera.position = m_world.spawn + vec3{0, 12.f, 22.f};
    m_camera.distance = 26.f;
    m_camera.pitch = -0.25f;

    // scripts
    m_api.startScripts(m_world);

    m_ready = true;
    RB_LOG_I("engine ready (%s mode, %zu chunks)",
             cfg.mode == AppMode::Player ? "player" : "studio", m_world.chunkCount());
    return true;
}

void Engine::frame(float dt) {
    // input
    m_window->pollEvents();
    m_input = m_window->takeInput();

    m_timeSec += dt;

    // adaptive quality (drives render distance / AO / resolution / meshing)
    m_quality.update(m_fps.smoothedMs);

    // scripts (RunService events + task.wait scheduler)
    m_api.update(dt, m_timeSec);

    // world + rendering
    m_renderer.update(m_world, m_camera, m_quality, m_timeSec);
    m_renderer.updateParticles(dt);

    // app logic
    onUpdate(dt);

    // UI overlay (HUD for player, panels for studio)
    m_ui.beginFrame(m_window->width(), m_window->height(), m_input);
    onRenderUI(m_ui, m_input);
    m_ui.endFrame();

    m_window->swapBuffers();

    // frame stats
    {
        float dtMs = dt * 1000.f;
        m_fps.tick(dtMs);
    }
    m_input.clearFrameFlags();

    if (m_window->shouldClose()) m_quit = true;
}

void Engine::run() {
    if (!m_ready) return;
    uint64_t last = nowMicros();
    while (!m_quit) {
        uint64_t now = nowMicros();
        float dt = (float)((now - last) / 1000000.0);
        last = now;
        if (dt <= 0.f) dt = 1.f / 60.f;
        if (dt > 0.1f) dt = 0.1f; // clamp big stalls (loading, GC)
        frame(dt);
    }
    onQuit();
    m_api.stopScripts();
    m_window->destroy();
    delete m_window;
    m_window = nullptr;
}

} // namespace rb

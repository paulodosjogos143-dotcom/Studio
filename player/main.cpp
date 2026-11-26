// ROXBOX Player - game runtime
// Loads a .roxmap (or generates the default world), runs scripts,
// drives a third-person character with voxel collision, HUD and mobile controls.
#include "../engine/engine.h"
#include "../engine/game/character.h"
#include "../engine/core/log.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace rb;

namespace {

class PlayerApp : public Engine {
public:
    bool init(const EngineConfig& cfg) override {
        if (!Engine::init(cfg)) return false;
        m_char.pos = findGround(world(), world().spawn + vec3{0.5f, 0.5f, 0.5f});
        m_char.vel = {0, 0, 0};
        camera().target = m_char.pos;
        camera().position = m_char.pos + vec3{0, 3.f, 7.f};
        camera().distance = 8.f;
        camera().pitch = -0.18f;
        return true;
    }

    void onUpdate(float dt) override {
        InputState& in = input();

        // script-driven teleport (game.Players.LocalPlayer.Position)
        vec3 teleport;
        if (api().consumePlayerTeleport(teleport)) {
            m_char.pos = teleport;
            m_char.vel = {0, 0, 0};
        }

        // camera control (mouse drag / touch right side / wheel)
        if (in.mouseDown[0] || in.mouseDown[1]) {
            camera().orbit((in.mouseX - m_lastMouseX) * 1.5f, (in.mouseY - m_lastMouseY) * 1.5f, 1.f);
        }
        if (in.wheel != 0.f) camera().orbit(0, 0, 1.f - in.wheel * 0.1f);
        m_lastMouseX = in.mouseX;
        m_lastMouseY = in.mouseY;

        // movement input (WASD / virtual joystick), camera-relative
        vec2 wish{0, 0};
        if (in.key(Key::W)) wish.y -= 1.f;
        if (in.key(Key::S)) wish.y += 1.f;
        if (in.key(Key::A)) wish.x -= 1.f;
        if (in.key(Key::D)) wish.x += 1.f;
        if (in.touchActive && (std::abs(in.joystick.x) > 0.1f || std::abs(in.joystick.y) > 0.1f)) {
            wish = {in.joystick.x, in.joystick.y};
        }
        float wl = std::sqrt(wish.x * wish.x + wish.y * wish.y);
        if (wl > 1.f) { wish.x /= wl; wish.y /= wl; }

        // camera forward (horizontal)
        vec3 camFwd = camera().forward();
        vec3 fwd = normalize(vec3{camFwd.x, 0, camFwd.z});
        vec3 right = normalize(cross(vec3{0, 1, 0}, fwd));
        vec3 wishV = fwd * (-wish.y) + right * wish.x;

        const float accel = 60.f, maxSpeed = 9.f, friction = 10.f;
        if (wishV.x != 0 || wishV.z != 0) {
            m_char.vel.x += wishV.x * accel * dt;
            m_char.vel.z += wishV.z * accel * dt;
        } else {
            float fr = friction * dt;
            if (std::abs(m_char.vel.x) <= fr) m_char.vel.x = 0; else m_char.vel.x -= std::signbit(m_char.vel.x) * fr;
            if (std::abs(m_char.vel.z) <= fr) m_char.vel.z = 0; else m_char.vel.z -= std::signbit(m_char.vel.z) * fr;
        }
        float hs = std::sqrt(m_char.vel.x * m_char.vel.x + m_char.vel.z * m_char.vel.z);
        if (hs > maxSpeed) { m_char.vel.x = m_char.vel.x / hs * maxSpeed; m_char.vel.z = m_char.vel.z / hs * maxSpeed; }

        // jump
        m_char.grounded = false;
        if ((in.pressed(Key::Space) || (in.touchActive && in.joystick.y < -0.5f && m_char.grounded)) && m_char.grounded)
            m_char.vel.y = 8.4f;

        // gravity + per-axis collision
        m_char.vel.y -= 24.f * dt;
        if (m_char.vel.y < -40.f) m_char.vel.y = -40.f;

        moveAxis(world(), m_char, 1, m_char.vel.y * dt);
        moveAxis(world(), m_char, 0, m_char.vel.x * dt);
        moveAxis(world(), m_char, 2, m_char.vel.z * dt);

        // fall out of the world -> respawn
        if (m_char.pos.y < -20.f) {
            m_char.pos = findGround(world(), world().spawn + vec3{0.5f, 0.5f, 0.5f});
            m_char.vel = {0, 0, 0};
        }

        // follow camera
        camera().target = m_char.pos + vec3{0, 1.f, 0};
        vec3 desired = camera().target + camera().forward() * camera().distance;
        camera().position = mix(camera().position, desired, std::min(1.f, dt * 8.f));

        // mirror position to scripts
        api().setPlayerPosition(m_char.pos);
    }

    void onRenderUI(UI& ui, const InputState& in) override {
        const RenderStats& rs = renderer().stats();
        float w = (float)window()->width(), h = (float)window()->height();
        vec4 white{1, 1, 1, 1}, dim{0.65f, 0.7f, 0.8f, 1.f}, accent{0.4f, 0.8f, 1.f, 1.f};

        // HUD (top-left)
        char buf[160];
        snprintf(buf, sizeof(buf), "FPS %.0f  tier: %s  pos %.0f,%.0f,%.0f",
                 fps().fps, quality().tierName(), m_char.pos.x, m_char.pos.y, m_char.pos.z);
        ui.drawText(10.f, 10.f, 1.5f, white, buf);
        snprintf(buf, sizeof(buf), "chunks %d  draws %d  tris %d  mem-chunks %zu",
                 rs.visibleChunks, rs.drawCalls, rs.triangles, world().chunkCount());
        ui.drawText(10.f, 26.f, 1.3f, dim, buf);

        // console (script output)
        float cy = h - 120.f;
        ui.fillRect(10.f, cy - 8.f, 420.f, 110.f, {0.05f, 0.07f, 0.1f, 0.7f});
        auto& console = api().console();
        size_t start = console.size() > 6 ? console.size() - 6 : 0;
        float ty = cy;
        for (size_t i = start; i < console.size(); ++i) {
            ui.drawText(14.f, ty, 1.2f, dim, console[i]);
            ty += 14.f;
        }

        // mobile controls
        if (in.touchActive || isMobile()) {
            // joystick visualization
            float jx = 60.f, jy = h - 90.f;
            ui.fillRect(jx - 40.f, jy - 40.f, 80.f, 80.f, {0.1f, 0.12f, 0.16f, 0.5f});
            ui.fillRect(jx + in.joystick.x * 30.f - 14.f, jy + in.joystick.y * 30.f - 14.f, 28.f, 28.f, accent);
            // jump button
            if (ui.button(w - 110.f, h - 110.f, 90.f, 60.f, "JUMP", in))
                input().justPressed[(int)Key::Space] = true;
        }

        // crosshair (center)
        ui.fillRect(w / 2 - 8.f, h / 2 - 1.f, 16.f, 2.f, {0.8f, 0.8f, 0.8f, 0.6f});
        ui.fillRect(w / 2 - 1.f, h / 2 - 8.f, 2.f, 16.f, {0.8f, 0.8f, 0.8f, 0.6f});
    }

    void setMobile(bool m) { m_mobile = m; }

private:
    Character m_char;
    float m_lastMouseX = 0.f, m_lastMouseY = 0.f;
    bool m_mobile = false;
    bool isMobile() const { return m_mobile; }
};

} // namespace

int main(int argc, char** argv) {
    rb::logSetLevel(rb::LogLevel::Info);
    rb::logSetFile("roxbox_player.log");

    PlayerApp app;
    EngineConfig cfg;
    cfg.mode = AppMode::Player;
    cfg.window.title = "ROXBOX Player";
    cfg.window.width = 1280;
    cfg.window.height = 720;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tier" && i + 1 < argc) {
            cfg.startTier = std::atoi(argv[++i]);
            cfg.autoQuality = false;
        } else if (a == "--auto") {
            cfg.autoQuality = true;
        } else if (a == "--mobile") {
            app.setMobile(true);
        } else if (a == "--width" && i + 1 < argc) {
            cfg.window.width = std::atoi(argv[++i]);
        } else if (a == "--height" && i + 1 < argc) {
            cfg.window.height = std::atoi(argv[++i]);
        } else if (a == "--fullscreen") {
            cfg.window.fullscreen = true;
        } else if (!a.empty() && a.find("--") != 0) {
            cfg.mapPath = a; // map file
        }
    }

    RB_LOG_I("ROXBOX Player starting...");
    if (!app.init(cfg)) {
        RB_LOG_E("init failed");
        return 1;
    }
    app.run();
    RB_LOG_I("ROXBOX Player exited");
    return 0;
}

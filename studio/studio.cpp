// ROXBOX Studio - editor implementation
#include "studio.h"
#include "../engine/core/fs.h"
#include "../engine/core/log.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

using namespace rb;

namespace {

const char* toolName(StudioApp::Tool t) {
    switch (t) {
        case StudioApp::Tool::Select: return "Select";
        case StudioApp::Tool::Build:  return "Build";
        case StudioApp::Tool::Paint:  return "Paint";
        case StudioApp::Tool::Erase:  return "Erase";
        case StudioApp::Tool::Move:   return "Move";
    }
    return "?";
}

const char* materialNames[] = {
    "Plastic", "NeonPlastic", "Wood", "Metal", "Neon", "Brick", "Glass",
    "Stone", "Sand", "Leaves", "Water", "Gold", "Diamond", "Redstone", "Lamp"
};

vec4 paletteColor(int idx) {
    if (idx <= 0) return {0.5f, 0.5f, 0.5f, 1.f};
    const RGB8& c = palette[idx];
    return {(float)c.r / 255.f, (float)c.g / 255.f, (float)c.b / 255.f, 1.f};
}

void drawLine(UI& ui, float x0, float y0, float x1, float y1, float thick, vec4 color) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.01f) return;
    float nx = -dy / len * thick * 0.5f, ny = dx / len * thick * 0.5f;
    ui.fillRect(std::min(x0, x1) + nx, std::min(y0, y1) + ny, std::abs(dx) + thick, thick, color);
}

} // namespace

bool StudioApp::init(const EngineConfig& cfg) {
    if (!Engine::init(cfg)) return false;
    camera().target = world().spawn;
    camera().position = world().spawn + vec3{0, 14.f, 26.f};
    camera().distance = 30.f;
    camera().pitch = -0.22f;
    m_scriptText.clear();
    return true;
}

// ---------------- unproject / project ----------------

vec3 StudioApp::unprojectRay(const InputState& in, vec3& dirOut) {
    float w = (float)window()->width(), h = (float)window()->height();
    float ndcX = (in.mouseX / w) * 2.f - 1.f;
    float ndcY = 1.f - (in.mouseY / h) * 2.f;
    mat4 vp = camera().projMatrix(w / h) * camera().viewMatrix();
    mat4 inv = vp.inverse();
    vec3 near = inv.transformPoint({ndcX, ndcY, 0.f});
    vec3 far = inv.transformPoint({ndcX, ndcY, 1.f});
    dirOut = normalize(far - near);
    return camera().position;
}

vec2 StudioApp::projectToScreen(const vec3& worldPos) {
    float w = (float)window()->width(), h = (float)window()->height();
    mat4 vp = camera().projMatrix(w / h) * camera().viewMatrix();
    vec3 clip = vp.transformPoint(worldPos);
    return {(clip.x * 0.5f + 0.5f) * w, (1.f - (clip.y * 0.5f + 0.5f)) * h};
}

// ---------------- update ----------------

void StudioApp::onUpdate(float dt) {
    if (m_playMode) updatePlayMode(dt);
    else updateEditMode(dt);
}

void StudioApp::updateEditMode(float dt) {
    InputState& in = input();
    bool uiFocused = ui().inputFocused();

    // hotkeys
    if (in.pressed(Key::Digit1)) m_tool = Tool::Select;
    if (in.pressed(Key::Digit2)) m_tool = Tool::Build;
    if (in.pressed(Key::Digit3)) m_tool = Tool::Paint;
    if (in.pressed(Key::Digit4)) m_tool = Tool::Erase;
    if (in.pressed(Key::Digit5)) m_tool = Tool::Move;
    if (in.pressed(Key::F5)) m_playMode = !m_playMode;
    if (in.pressed(Key::Escape)) m_playMode = false;

    handleViewportInput(in, dt);
    if (m_playMode) return;

    handleToolAction(in);
}

void StudioApp::handleViewportInput(const InputState& in, float dt) {
    bool uiFocused = ui().inputFocused();

    // camera orbit with mouse drag (right or left drag in viewport area)
    float w = (float)window()->width(), h = (float)window()->height();
    bool inViewport = in.mouseX < w * 0.72f && in.mouseY > 44.f && in.mouseY < h - 26.f;

    if (inViewport && (in.mouseDown[0] || in.mouseDown[1])) {
        camera().orbit((in.mouseX - m_lastMouseX) * 1.2f, (in.mouseY - m_lastMouseY) * 1.2f, 1.f);
    }
    if (in.wheel != 0.f) camera().orbit(0, 0, 1.f - in.wheel * 0.12f);
    m_lastMouseX = in.mouseX;
    m_lastMouseY = in.mouseY;

    if (!uiFocused && inViewport) {
        if (in.key(Key::W)) camera().fly(1.f, 0, 0);
        if (in.key(Key::S)) camera().fly(-1.f, 0, 0);
        if (in.key(Key::A)) camera().fly(0, -1.f, 0);
        if (in.key(Key::D)) camera().fly(0, 1.f, 0);
        if (in.key(Key::Q) || in.key(Key::Shift)) camera().fly(0, 0, -1.f);
        if (in.key(Key::E) || in.key(Key::Control)) camera().fly(0, 0, 1.f);
    }

    // raycast for targeting
    if (inViewport) {
        vec3 dir;
        vec3 origin = unprojectRay(in, dir);
        m_lastHit = raycastVoxel(world(), origin, dir, 250.f);
    } else {
        m_lastHit.hit = false;
    }
}

void StudioApp::handleToolAction(const InputState& in) {
    float w = (float)window()->width(), h = (float)window()->height();
    bool inViewport = in.mouseX < w * 0.72f && in.mouseY > 44.f && in.mouseY < h - 26.f;
    if (!inViewport) return;
    if (!m_lastHit.hit) return;

    if (in.mouseClicked[0]) {
        switch (m_tool) {
            case Tool::Build: {
                vec3 target = m_lastHit.voxel + m_lastHit.normal;
                uint8_t block = materialToBlock((Material)m_material);
                world().setBlock((int)target.x, (int)target.y, (int)target.z, block);
                if (m_color > 0) world().setTint((int)target.x, (int)target.y, (int)target.z, (uint8_t)m_color);
                break;
            }
            case Tool::Paint: {
                Part* p = world().findPartAt((int)m_lastHit.voxel.x, (int)m_lastHit.voxel.y, (int)m_lastHit.voxel.z);
                if (p) {
                    api().recolorPartCpp(*p, m_color);
                } else if (m_color > 0) {
                    world().setTint((int)m_lastHit.voxel.x, (int)m_lastHit.voxel.y, (int)m_lastHit.voxel.z, (uint8_t)m_color);
                }
                break;
            }
            case Tool::Erase: {
                Part* p = world().findPartAt((int)m_lastHit.voxel.x, (int)m_lastHit.voxel.y, (int)m_lastHit.voxel.z);
                if (p) {
                    world().removePart(p->id);
                    if (m_selectedPartId == p->id) m_selectedPartId = -1;
                    api().startScripts(world()); // refresh script bindings
                } else {
                    world().setBlock((int)m_lastHit.voxel.x, (int)m_lastHit.voxel.y, (int)m_lastHit.voxel.z, BLK_AIR);
                    world().setTint((int)m_lastHit.voxel.x, (int)m_lastHit.voxel.y, (int)m_lastHit.voxel.z, 0);
                }
                break;
            }
            case Tool::Select: {
                Part* p = world().findPartAt((int)m_lastHit.voxel.x, (int)m_lastHit.voxel.y, (int)m_lastHit.voxel.z);
                m_selectedPartId = p ? p->id : -1;
                if (p) m_scriptText = p->scriptSource;
                break;
            }
            case Tool::Move: {
                Part* p = world().findPartAt((int)m_lastHit.voxel.x, (int)m_lastHit.voxel.y, (int)m_lastHit.voxel.z);
                if (p) {
                    m_selectedPartId = p->id;
                    m_draggingMove = true;
                    m_dragStartPos = p->position;
                    m_dragStartScreen = {in.mouseX, in.mouseY};
                }
                break;
            }
        }
    }

    // drag to build a line / move a part
    if (in.mouseDown[0] && inViewport) {
        if (m_tool == Tool::Build && m_lastHit.hit) {
            vec3 target = m_lastHit.voxel + m_lastHit.normal;
            uint8_t block = materialToBlock((Material)m_material);
            world().setBlock((int)target.x, (int)target.y, (int)target.z, block);
            if (m_color > 0) world().setTint((int)target.x, (int)target.y, (int)target.z, (uint8_t)m_color);
        }
        if (m_tool == Tool::Paint && m_lastHit.hit) {
            if (m_color > 0)
                world().setTint((int)m_lastHit.voxel.x, (int)m_lastHit.voxel.y, (int)m_lastHit.voxel.z, (uint8_t)m_color);
        }
    }

    if (m_draggingMove && !in.mouseDown[0]) {
        m_draggingMove = false;
    }
    if (m_draggingMove && m_selectedPartId > 0) {
        // project the screen delta onto the camera-facing plane through the part
        Part* p = world().findPart(m_selectedPartId);
        if (p) {
            vec3 center = p->position + p->size * 0.5f;
            vec3 dir;
            vec3 origin = unprojectRay(in, dir);
            vec3 camRight = camera().right();
            vec3 camUp = normalize(cross(dir, camRight));
            vec2 cur = projectToScreen(center);
            vec2 d = {in.mouseX - m_dragStartScreen.x, in.mouseY - m_dragStartScreen.y};
            float scale = 0.02f * camera().distance;
            vec3 delta = camRight * (-d.x * scale) + camUp * (d.y * scale);
            api().movePartCpp(*p, m_dragStartPos + delta);
        }
    }
}

void StudioApp::updatePlayMode(float dt) {
    InputState& in = input();
    ui().clearFocus();

    // teleport from scripts
    vec3 teleport;
    if (api().consumePlayerTeleport(teleport)) {
        m_character.pos = teleport;
        m_character.vel = {0, 0, 0};
    }

    // camera orbit with mouse
    if (in.mouseDown[0] || in.mouseDown[1]) {
        camera().orbit((in.mouseX - m_lastMouseX) * 1.5f, (in.mouseY - m_lastMouseY) * 1.5f, 1.f);
    }
    if (in.wheel != 0.f) camera().orbit(0, 0, 1.f - in.wheel * 0.1f);
    m_lastMouseX = in.mouseX;
    m_lastMouseY = in.mouseY;

    // movement
    vec2 wish{0, 0};
    if (in.key(Key::W)) wish.y -= 1.f;
    if (in.key(Key::S)) wish.y += 1.f;
    if (in.key(Key::A)) wish.x -= 1.f;
    if (in.key(Key::D)) wish.x += 1.f;
    if (in.touchActive) wish = {in.joystick.x, in.joystick.y};
    float wl = std::sqrt(wish.x * wish.x + wish.y * wish.y);
    if (wl > 1.f) { wish.x /= wl; wish.y /= wl; }

    vec3 camFwd = normalize(vec3{camera().forward().x, 0, camera().forward().z});
    vec3 right = normalize(cross(vec3{0, 1, 0}, camFwd));
    vec3 wishV = camFwd * (-wish.y) + right * wish.x;

    const float accel = 60.f, maxSpeed = 9.f, friction = 10.f;
    if (wishV.x != 0 || wishV.z != 0) {
        m_character.vel.x += wishV.x * accel * dt;
        m_character.vel.z += wishV.z * accel * dt;
    } else {
        float fr = friction * dt;
        if (std::abs(m_character.vel.x) <= fr) m_character.vel.x = 0; else m_character.vel.x -= std::signbit(m_character.vel.x) * fr;
        if (std::abs(m_character.vel.z) <= fr) m_character.vel.z = 0; else m_character.vel.z -= std::signbit(m_character.vel.z) * fr;
    }
    float hs = std::sqrt(m_character.vel.x * m_character.vel.x + m_character.vel.z * m_character.vel.z);
    if (hs > maxSpeed) { m_character.vel.x = m_character.vel.x / hs * maxSpeed; m_character.vel.z = m_character.vel.z / hs * maxSpeed; }

    m_character.grounded = false;
    if (in.pressed(Key::Space) && m_character.grounded) m_character.vel.y = 8.4f;
    m_character.vel.y -= 24.f * dt;

    moveAxis(world(), m_character, 1, m_character.vel.y * dt);
    moveAxis(world(), m_character, 0, m_character.vel.x * dt);
    moveAxis(world(), m_character, 2, m_character.vel.z * dt);
    if (m_character.pos.y < -20.f) {
        m_character.pos = findGround(world(), world().spawn + vec3{0.5f, 0.5f, 0.5f});
        m_character.vel = {0, 0, 0};
    }

    camera().target = m_character.pos + vec3{0, 1.f, 0};
    vec3 desired = camera().target + camera().forward() * camera().distance;
    camera().position = mix(camera().position, desired, std::min(1.f, dt * 8.f));

    api().setPlayerPosition(m_character.pos);
}

// ---------------- render UI ----------------

void StudioApp::onRenderUI(UI& ui, const InputState& in) {
    float w = (float)window()->width(), h = (float)window()->height();
    vec4 white{0.95f, 0.96f, 0.98f, 1.f};
    vec4 dim{0.6f, 0.65f, 0.72f, 1.f};
    vec4 accent{0.35f, 0.75f, 1.f, 1.f};

    if (m_playMode) {
        // play HUD
        ui.fillRect(0, 0, w, 40.f, {0.05f, 0.07f, 0.1f, 0.8f});
        ui.drawText(12.f, 12.f, 1.5f, white, "PLAY MODE - press F5 or click STOP");
        if (ui.button(w - 140.f, 8.f, 120.f, 26.f, "STOP", in)) m_playMode = false;

        char buf[160];
        snprintf(buf, sizeof(buf), "FPS %.0f  tier %s  pos %.0f,%.0f,%.0f",
                 fps().fps, quality().tierName(), m_character.pos.x, m_character.pos.y, m_character.pos.z);
        ui.drawText(12.f, 44.f, 1.4f, white, buf);
        drawViewportDecorations(ui);
        return;
    }

    drawViewportDecorations(ui);
    drawToolbar(ui, in);

    // right panel
    float px = w * 0.72f, pw = w - px - 8.f;
    drawWorldPanel(ui, in, px, pw);
    drawPropertiesPanel(ui, in, px, pw);
    drawScriptPanel(ui, in, px, pw);
    drawSettingsPanel(ui, in, px, pw);
    drawStatusBar(ui);
}

void StudioApp::drawViewportDecorations(UI& ui) {
    float w = (float)window()->width(), h = (float)window()->height();
    vec4 gridColor{0.35f, 0.5f, 0.7f, 0.35f};
    vec4 selColor{1.f, 0.85f, 0.2f, 1.f};

    // ground grid (chunk-aligned, around the camera target)
    if (m_showGrid) {
        vec3 center = camera().target;
        int groundY = (int)std::floor(center.y);
        for (int y = 0; y < 80; ++y) {
            int gy = (int)center.y - y;
            if (blockIsSolid(world().getBlock((int)center.x, gy, (int)center.z))) { groundY = gy; break; }
        }
        float gY = (float)groundY;
        float radius = 60.f;
        float step = 8.f;
        int x0 = (int)std::floor((center.x - radius) / step) * (int)step;
        int z0 = (int)std::floor((center.z - radius) / step) * (int)step;
        for (int x = x0; x <= (int)center.x + radius; x += (int)step) {
            vec2 a = projectToScreen({(float)x, gY, center.z - radius});
            vec2 b = projectToScreen({(float)x, gY, center.z + radius});
            drawLine(ui, a.x, a.y, b.x, b.y, 1.f, gridColor);
        }
        for (int z = z0; z <= (int)center.z + radius; z += (int)step) {
            vec2 a = projectToScreen({center.x - radius, gY, (float)z});
            vec2 b = projectToScreen({center.x + radius, gY, (float)z});
            drawLine(ui, a.x, a.y, b.x, b.y, 1.f, gridColor);
        }
    }

    // selection box
    if (m_selectedPartId > 0) {
        Part* p = world().findPart(m_selectedPartId);
        if (p) {
            aabb box = aabb::make(p->position, p->position + p->size);
            vec3 corners[8];
            int ci = 0;
            for (int sx = 0; sx < 2; ++sx)
                for (int sy = 0; sy < 2; ++sy)
                    for (int sz = 0; sz < 2; ++sz)
                        corners[ci++] = {box.minv.x + sx * (box.maxv.x - box.minv.x),
                                         box.minv.y + sy * (box.maxv.y - box.minv.y),
                                         box.minv.z + sz * (box.maxv.z - box.minv.z)};
            int edges[12][2] = {{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
            for (auto& e : edges) {
                vec2 a = projectToScreen(corners[e[0]]);
                vec2 b = projectToScreen(corners[e[1]]);
                drawLine(ui, a.x, a.y, b.x, b.y, 2.f, selColor);
            }
        }
    }

    // crosshair + hit marker
    float cx = w * 0.5f, cy = h * 0.55f;
    if (m_lastHit.hit) {
        vec2 hit = projectToScreen(m_lastHit.voxel + vec3{0.5f, 0.5f, 0.5f});
        ui.fillRect(hit.x - 8.f, hit.y - 1.f, 16.f, 2.f, {1.f, 0.3f, 0.3f, 1.f});
        ui.fillRect(hit.x - 1.f, hit.y - 8.f, 2.f, 16.f, {1.f, 0.3f, 0.3f, 1.f});
    }
    ui.fillRect(cx - 10.f, cy - 1.f, 20.f, 2.f, {0.8f, 0.8f, 0.8f, 0.5f});
    ui.fillRect(cx - 1.f, cy - 10.f, 2.f, 20.f, {0.8f, 0.8f, 0.8f, 0.5f});
}

void StudioApp::drawToolbar(UI& ui, const InputState& in) {
    float w = (float)window()->width();
    ui.fillRect(0, 0, w, 40.f, {0.08f, 0.10f, 0.14f, 0.95f});
    float x = 8.f;
    const float bw = 90.f, bh = 28.f;

    struct TB { const char* label; StudioApp::Tool tool; };
    TB tools[] = {{"1 Select", Tool::Select}, {"2 Build", Tool::Build}, {"3 Paint", Tool::Paint},
                  {"4 Erase", Tool::Erase}, {"5 Move", Tool::Move}};
    for (auto& t : tools) {
        bool active = m_tool == t.tool;
        vec4 col = active ? vec4{0.35f, 0.75f, 1.f, 1.f} : vec4{0.16f, 0.18f, 0.22f, 1.f};
        ui.fillRect(x, 6.f, bw, bh, col);
        ui.drawText(x + 6.f, 12.f, 1.3f, active ? vec4{0.1f, 0.2f, 0.3f, 1.f} : vec4{0.85f, 0.88f, 0.92f, 1.f}, t.label);
        if (active && in.mouseClicked[0] && in.mouseX >= x && in.mouseX <= x + bw && in.mouseY >= 6.f && in.mouseY <= 6.f + bh)
            ui.clearFocus();
        x += bw + 4.f;
    }

    x += 8.f;
    if (ui.button(x, 6.f, 100.f, bh, m_playMode ? "PLAY" : "PLAY", in)) m_playMode = true;
    x += 104.f;
    if (ui.button(x, 6.f, 90.f, bh, "SAVE", in)) {
        if (fs::ensureDir("saves")) {
            if (world().saveToFile(m_savePath)) RB_LOG_I("saved to %s", m_savePath.c_str());
        }
    }
    x += 94.f;
    if (ui.button(x, 6.f, 90.f, bh, "LOAD", in)) {
        if (world().loadFromFile(m_loadPath)) {
            api().startScripts(world());
            m_selectedPartId = -1;
            RB_LOG_I("loaded %s", m_loadPath.c_str());
        }
    }
    x += 94.f;
    if (ui.button(x, 6.f, 90.f, bh, "NEW WORLD", in)) {
        world().clear();
        generateDefaultWorld(world());
        api().startScripts(world());
        m_selectedPartId = -1;
        m_scriptText.clear();
    }
}

void StudioApp::drawWorldPanel(UI& ui, const InputState& in, float px, float pw) {
    float x, y, cw, ch;
    if (!ui.window(px, 48.f, pw, 200.f, "WORLD", in, &x, &y, &cw, &ch)) return;

    ui.drawText(x + 6.f, y + 2.f, 1.3f, vec4{0.7f, 0.75f, 0.82f, 1.f},
                "parts: " + std::to_string(world().parts().size()));

    float rowY = y + 18.f;
    float rowH = 16.f;
    int i = 0;
    for (Part& p : world().parts()) {
        bool selected = p.id == m_selectedPartId;
        (void)selected;
        vec4 bg = selected ? vec4{0.30f, 0.55f, 0.8f, 0.5f} : vec4{0.12f, 0.14f, 0.18f, 0.4f};
        ui.fillRect(x + 4.f, rowY, cw - 8.f, rowH, bg);
        char buf[128];
        snprintf(buf, sizeof(buf), "#%d  %s  (%.0f,%.0f,%.0f)", p.id, p.name.c_str(),
                 p.position.x, p.position.y, p.position.z);
        ui.drawText(x + 10.f, rowY + 2.f, 1.2f, vec4{0.9f, 0.92f, 0.96f, 1.f}, buf);
        if (in.mouseClicked[0] && in.mouseY >= rowY && in.mouseY <= rowY + rowH &&
            in.mouseX >= x + 4.f && in.mouseX <= x + cw - 4.f) {
            m_selectedPartId = p.id;
            m_scriptText = p.scriptSource;
            ui.clearFocus();
        }
        rowY += rowH + 2.f;
        ++i;
        if (i > 10) {
            ui.drawText(x + 6.f, rowY, 1.2f, vec4{0.5f, 0.55f, 0.6f, 1.f}, "...");
            break;
        }
    }

    if (ui.button(x + 6.f, y + ch - 30.f, 110.f, 22.f, "+ ADD PART", in)) {
        Part& p = world().addPart("Part", world().spawn, vec3{2, 2, 2}, (Material)m_material, m_color);
        m_selectedPartId = p.id;
        m_scriptText = p.scriptSource;
        api().startScripts(world());
    }
    if (ui.button(x + 122.f, y + ch - 30.f, 90.f, 22.f, "DELETE", in)) {
        if (m_selectedPartId > 0) {
            world().removePart(m_selectedPartId);
            m_selectedPartId = -1;
            api().startScripts(world());
        }
    }
}

void StudioApp::drawPropertiesPanel(UI& ui, const InputState& in, float px, float pw) {
    float x, y, cw, ch;
    if (!ui.window(px, 260.f, pw, 260.f, "PROPERTIES", in, &x, &y, &cw, &ch)) return;

    Part* p = m_selectedPartId > 0 ? world().findPart(m_selectedPartId) : nullptr;
    if (!p) {
        ui.drawText(x + 8.f, y + 8.f, 1.3f, vec4{0.5f, 0.55f, 0.6f, 1.f}, "no part selected");
        return;
    }

    float cy = y + 4.f;
    ui.drawText(x + 6.f, cy, 1.3f, vec4{0.8f, 0.85f, 0.9f, 1.f}, std::string("#") + std::to_string(p->id));
    cy += 16.f;

    // name
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, "Name");
    static std::string nameBuf;
    if (!ui.inputFocused()) nameBuf = p->name;
    ui.textInput(x + 6.f, cy + 12.f, cw - 12.f, 18.f, nameBuf, in);
    // apply name when editing is done (focus lost)
    if (!ui.inputFocused() && nameBuf != p->name) {
        p->name = nameBuf;
        api().notifyPartChanged(p, "Name");
    }
    cy += 36.f;

    // position
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, "Position");
    ui.slider(x + 6.f, cy + 12.f, cw * 0.45f, p->position.x, -64, 256, "%.0f", in);
    ui.slider(x + 6.f, cy + 28.f, cw * 0.45f, p->position.y, -64, 256, "%.0f", in);
    ui.slider(x + 6.f, cy + 44.f, cw * 0.45f, p->position.z, -64, 256, "%.0f", in);
    // apply position live when sliders were dragged (mouse down on slider)
    if (in.mouseDown[0] && in.mouseY > cy + 8.f && in.mouseY < cy + 52.f && in.mouseX < x + cw * 0.5f) {
        api().movePartCpp(*p, p->position);
    }
    cy += 60.f;

    // size
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, "Size");
    ui.slider(x + 6.f, cy + 12.f, cw * 0.45f, p->size.x, 1, 32, "%.0f", in);
    ui.slider(x + 6.f, cy + 28.f, cw * 0.45f, p->size.y, 1, 32, "%.0f", in);
    ui.slider(x + 6.f, cy + 44.f, cw * 0.45f, p->size.z, 1, 32, "%.0f", in);
    if (in.mouseDown[0] && in.mouseY > cy + 8.f && in.mouseY < cy + 52.f && in.mouseX < x + cw * 0.5f) {
        api().resizePartCpp(*p, p->size);
    }
    cy += 60.f;

    // color palette
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, "Color");
    float swatch = 16.f;
    for (int i = 0; i < PALETTE_SIZE; ++i) {
        float sx = x + 6.f + (float)(i % 8) * (swatch + 3.f);
        float sy = cy + 12.f + (float)(i / 8) * (swatch + 3.f);
        ui.fillRect(sx, sy, swatch, swatch, paletteColor(i + 1));
        if (i + 1 == p->colorPalette) {
            ui.fillRect(sx - 1.f, sy - 1.f, swatch + 2.f, 1.f, vec4{1.f, 0.85f, 0.2f, 1.f});
            ui.fillRect(sx - 1.f, sy + swatch, swatch + 2.f, 1.f, vec4{1.f, 0.85f, 0.2f, 1.f});
            ui.fillRect(sx - 1.f, sy - 1.f, 1.f, swatch + 2.f, vec4{1.f, 0.85f, 0.2f, 1.f});
            ui.fillRect(sx + swatch, sy - 1.f, 1.f, swatch + 2.f, vec4{1.f, 0.85f, 0.2f, 1.f});
        }
        if (in.mouseClicked[0] && in.mouseX >= sx && in.mouseX <= sx + swatch && in.mouseY >= sy && in.mouseY <= sy + swatch) {
            m_color = i + 1;
            api().recolorPartCpp(*p, i + 1);
        }
    }
    m_color = p->colorPalette;
    cy += 16.f + swatch * 3.f + 10.f;

    // material
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, "Material");
    float my = cy + 12.f;
    for (int i = 0; i < 15; ++i) {
        float mx = x + 6.f + (float)(i % 3) * 70.f;
        float mmy = my + (float)(i / 3) * 22.f;
        vec4 col = (i == (int)p->material) ? vec4{0.35f, 0.75f, 1.f, 1.f} : vec4{0.14f, 0.16f, 0.20f, 1.f};
        ui.fillRect(mx, mmy, 64.f, 18.f, col);
        ui.drawText(mx + 4.f, mmy + 2.f, 1.1f, vec4{0.85f, 0.88f, 0.92f, 1.f}, materialNames[i]);
        if (in.mouseClicked[0] && in.mouseX >= mx && in.mouseX <= mx + 64.f && in.mouseY >= mmy && in.mouseY <= mmy + 18.f) {
            m_material = i;
            api().rematerializePartCpp(*p, (Material)i);
        }
    }
    cy = my + 5 * 22.f + 8.f;

    // transparency / flags
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, "Transparency");
    ui.slider(x + 6.f, cy + 12.f, cw * 0.6f, p->transparency, 0.f, 1.f, "%.2f", in);
    p->transparency = std::max(0.f, std::min(1.f, p->transparency));
    cy += 32.f;
    ui.checkbox(x + 6.f, cy, "CanCollide", p->canCollide, in);
    ui.checkbox(x + 6.f + 120.f, cy, "Anchored", p->anchored, in);
}

void StudioApp::drawScriptPanel(UI& ui, const InputState& in, float px, float pw) {
    float x, y, cw, ch;
    if (!ui.window(px, 524.f, pw, 280.f, "SCRIPT (LUA)", in, &x, &y, &cw, &ch)) return;

    Part* p = m_selectedPartId > 0 ? world().findPart(m_selectedPartId) : nullptr;
    if (!p) {
        ui.drawText(x + 8.f, y + 8.f, 1.3f, vec4{0.5f, 0.55f, 0.6f, 1.f}, "select a part to edit its script");
        return;
    }
    if (m_scriptText != p->scriptSource) {
        // keep the editor text in sync when selection changes elsewhere
        if (!ui.inputFocused()) m_scriptText = p->scriptSource;
    }
    ui.textEditor(x + 6.f, y + 4.f, cw - 12.f, ch - 44.f, m_scriptText, in);

    if (ui.button(x + 6.f, y + ch - 34.f, 90.f, 24.f, "APPLY SCRIPT", in)) {
        p->scriptSource = m_scriptText;
        api().startScripts(world());
        RB_LOG_I("script applied to %s", p->name.c_str());
    }
    if (ui.button(x + 102.f, y + ch - 34.f, 90.f, 24.f, "RUN PLAY", in)) {
        p->scriptSource = m_scriptText;
        api().startScripts(world());
        m_playMode = true;
    }
}

void StudioApp::drawSettingsPanel(UI& ui, const InputState& in, float px, float pw) {
    float x, y, cw, ch;
    if (!ui.window(px, 816.f, pw, 170.f, "SETTINGS / QUALITY", in, &x, &y, &cw, &ch)) return;

    float cy = y + 6.f;
    bool autoQ = quality().mode == QualityManager::Mode::Auto;
    ui.checkbox(x + 6.f, cy, "Auto quality (60 FPS target)", autoQ, in);
    if (in.mouseClicked[0] && !autoQ) quality().setAuto();
    if (in.mouseClicked[0] && autoQ) quality().setManual(quality().tier);
    cy += 24.f;

    int tier = quality().tier;
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, "Tier");
    float tierF = (float)tier;
    ui.slider(x + 6.f, cy + 12.f, cw - 12.f, tierF, 0.f, 3.f, "%.0f", in);
    if (in.mouseDown[0] && in.mouseY > cy + 8.f && in.mouseY < cy + 24.f) {
        quality().setManual((int)(tierF + 0.5f));
        tier = quality().tier;
    }
    cy += 36.f;

    char buf[160];
    snprintf(buf, sizeof(buf), "dist %d  res %.2f  mesh/frame %d  fog %.0f",
             quality().renderDistance(), quality().resolutionScale(),
             quality().meshChunksPerFrame(), quality().fogFar());
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, buf);
    cy += 18.f;

    snprintf(buf, sizeof(buf), "save: %s", m_savePath.c_str());
    ui.textInput(x + 6.f, cy, cw - 12.f, 18.f, m_savePath, in);
    cy += 24.f;
    ui.drawText(x + 6.f, cy, 1.2f, vec4{0.6f, 0.65f, 0.72f, 1.f}, "load:");
    ui.textInput(x + 6.f, cy + 12.f, cw - 12.f, 18.f, m_loadPath, in);
}

void StudioApp::drawStatusBar(UI& ui) {
    float w = (float)window()->width(), h = (float)window()->height();
    ui.fillRect(0, h - 26.f, w, 26.f, {0.08f, 0.10f, 0.14f, 0.95f});
    const RenderStats& rs = renderer().stats();
    char buf[200];
    snprintf(buf, sizeof(buf), "tool: %s   |   FPS %.0f  tier %s   chunks %d (dirty %zu)   draws %d   tris %d",
             toolName(m_tool), fps().fps, quality().tierName(),
             rs.visibleChunks, world().dirtyCount(), rs.drawCalls, rs.triangles);
    ui.drawText(8.f, h - 20.f, 1.2f, vec4{0.75f, 0.8f, 0.88f, 1.f}, buf);
    if (m_lastHit.hit) {
        snprintf(buf, sizeof(buf), "target voxel %.0f,%.0f,%.0f  n %.0f,%.0f,%.0f",
                 m_lastHit.voxel.x, m_lastHit.voxel.y, m_lastHit.voxel.z,
                 m_lastHit.normal.x, m_lastHit.normal.y, m_lastHit.normal.z);
        ui.drawText(w - 320.f, h - 20.f, 1.2f, vec4{0.75f, 0.8f, 0.88f, 1.f}, buf);
    }
}

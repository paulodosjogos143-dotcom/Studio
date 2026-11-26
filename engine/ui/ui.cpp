// ROXBOX - immediate-mode UI implementation
#include "ui.h"
#include "font.h"
#include "../core/log.h"
#include <cmath>
#include <cstring>
#include <algorithm>

#if defined(ROXBOX_HEADLESS)
#  include "../render/gl_stub/gl.h"
#elif defined(ROXBOX_GLES2)
#  include <GLES2/gl2.h>
#else
#  include <GL/gl.h>
#endif

namespace rb {

namespace {

const char* uiVertexShader() {
#if defined(ROXBOX_GLES2) || defined(ROXBOX_HEADLESS)
    return R"GLSL(
attribute vec2 aPos;
attribute vec2 aUV;
attribute vec4 aColor;
uniform vec2 uScreen;
varying vec2 vUV;
varying vec4 vColor;
void main() {
    vec2 clip = (aPos / uScreen) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)GLSL";
#else
    return R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vColor;
void main() {
    vec2 clip = (aPos / uScreen) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)GLSL";
#endif
}

const char* uiFragmentShader() {
#if defined(ROXBOX_GLES2) || defined(ROXBOX_HEADLESS)
    return R"GLSL(
precision mediump float;
uniform sampler2D uFont;
uniform float uMode;
varying vec2 vUV;
varying vec4 vColor;
void main() {
    if (uMode < 0.5) {
        gl_FragColor = vColor;
    } else {
        float a = texture2D(uFont, vUV).a;
        gl_FragColor = vec4(vColor.rgb, vColor.a * a);
    }
}
)GLSL";
#else
    return R"GLSL(
#version 330 core
precision highp float;
uniform sampler2D uFont;
uniform float uMode;
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
void main() {
    if (uMode < 0.5) {
        fragColor = vColor;
    } else {
        float a = texture(uFont, vUV).a;
        fragColor = vec4(vColor.rgb, vColor.a * a);
    }
}
)GLSL";
#endif
}

inline bool pointInRect(const InputState& in, float x, float y, float w, float h) {
    return in.mouseX >= x && in.mouseX <= x + w && in.mouseY >= y && in.mouseY <= y + h;
}

} // namespace

bool UI::init(GLBackend& backend, int width, int height) {
    (void)backend; // backend is bound in the constructor
    m_width = width;
    m_height = height;
    std::string err;
    m_program = m_backend.createProgram(uiVertexShader(), uiFragmentShader(), err);
    if (!m_program) {
        RB_LOG_E("UI program failed: %s", err.c_str());
        return false;
    }
    std::vector<uint8_t> atlas = generateFontAtlas();
    m_font = m_backend.createTexture(FONT_ATLAS_SIZE, FONT_ATLAS_SIZE, atlas.data());
    return m_font != 0;
}

void UI::beginFrame(int width, int height, const InputState& in) {
    m_width = width;
    m_height = height;
    m_in = &in;
    m_quads.clear();
}

void UI::addQuad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                 float r, float g, float b, float a, bool text) {
    if (m_quads.size() >= 8192) return;
    m_quads.push_back({x, y, w, h, u0, v0, u1, v1, r, g, b, a, text});
}

void UI::fillRect(float x, float y, float w, float h, vec4 color) {
    addQuad(x, y, w, h, 0, 0, 1, 1, color.x, color.y, color.z, color.w, false);
}

float UI::textWidth(const std::string& s, float scale) const {
    return (float)s.size() * FONT_GLYPH_W * scale;
}

void UI::drawText(float x, float y, float scale, vec4 color, const std::string& s) {
    float cx = x;
    for (char c : s) {
        int g = fontGlyphIndex(c);
        int gx = g % 21, gy = g / 21;
        float u0 = (float)(gx * FONT_GLYPH_W) / FONT_ATLAS_SIZE;
        float v0 = (float)(gy * FONT_GLYPH_H) / FONT_ATLAS_SIZE;
        float u1 = u0 + (float)FONT_GLYPH_W / FONT_ATLAS_SIZE;
        float v1 = v0 + (float)FONT_GLYPH_H / FONT_ATLAS_SIZE;
        float w = FONT_GLYPH_W * scale, h = FONT_GLYPH_H * scale;
        addQuad(cx, y, w, h, u0, v0, u1, v1, color.x, color.y, color.z, color.w, true);
        cx += w;
    }
}

void UI::endFrame() {
    if (m_quads.empty()) return;
    // mouse up anywhere clears drag/focus-click state
    if (m_in && (m_in->mouseClicked[0] || m_in->mouseClicked[1] || m_in->mouseClicked[2])) {
        // handled per-widget; nothing global needed
    }

    m_backend.useProgram(m_program);
    m_backend.bindTexture(0, m_font);
    m_backend.setUniform1i("uFont", 0);
    m_backend.setUniform2f("uScreen", (float)m_width, (float)m_height);

    // build interleaved buffer: pos(2) uv(2) color(4)
    std::vector<float> buf;
    buf.reserve(m_quads.size() * 8);
    for (const Quad& q : m_quads) {
        buf.push_back(q.x); buf.push_back(q.y);
        buf.push_back(q.u0); buf.push_back(q.v0);
        buf.push_back(q.r); buf.push_back(q.g); buf.push_back(q.b); buf.push_back(q.a);
        buf.push_back(q.x + q.w); buf.push_back(q.y);
        buf.push_back(q.u1); buf.push_back(q.v0);
        buf.push_back(q.r); buf.push_back(q.g); buf.push_back(q.b); buf.push_back(q.a);
        buf.push_back(q.x + q.w); buf.push_back(q.y + q.h);
        buf.push_back(q.u1); buf.push_back(q.v1);
        buf.push_back(q.r); buf.push_back(q.g); buf.push_back(q.b); buf.push_back(q.a);
        buf.push_back(q.x); buf.push_back(q.y + q.h);
        buf.push_back(q.u0); buf.push_back(q.v1);
        buf.push_back(q.r); buf.push_back(q.g); buf.push_back(q.b); buf.push_back(q.a);
    }

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(buf.size() * sizeof(float)), buf.data(), GL_STREAM_DRAW);

    // solid pass
    m_backend.setUniform1f("uMode", 0.f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (const void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (const void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (const void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(m_quads.size() * 6));

    // text pass (re-upload only glyph quads would be better; one buffer is fine)
    m_backend.setUniform1f("uMode", 1.f);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(m_quads.size() * 6));

    glDeleteBuffers(1, &vbo);
}

bool UI::button(float x, float y, float w, float h, const std::string& label, const InputState& in) {
    bool hover = pointInRect(in, x, y, w, h);
    vec4 bg = hover ? vec4{0.25f, 0.27f, 0.32f, 1.f} : vec4{0.16f, 0.18f, 0.22f, 1.f};
    fillRect(x, y, w, h, bg);
    vec4 fg = hover ? vec4{0.95f, 0.95f, 0.98f, 1.f} : vec4{0.75f, 0.78f, 0.85f, 1.f};
    drawText(x + 4.f, y + (h - FONT_GLYPH_H * 1.5f) / 2.f, 1.5f, fg, label);
    if (hover && in.mouseClicked[0]) {
        if (m_focused) m_focused = false;
        return true;
    }
    return false;
}

bool UI::checkbox(float x, float y, const std::string& label, bool& state, const InputState& in) {
    float boxW = 12.f, boxH = 12.f;
    bool hover = pointInRect(in, x, y, boxW, boxH);
    vec4 bg = state ? vec4{0.30f, 0.65f, 0.40f, 1.f} : vec4{0.12f, 0.14f, 0.18f, 1.f};
    fillRect(x, y, boxW, boxH, bg);
    if (hover && in.mouseClicked[0]) state = !state;
    drawText(x + boxW + 5.f, y - 1.f, 1.4f, vec4{0.85f, 0.85f, 0.9f, 1.f}, label);
    return hover && in.mouseClicked[0];
}

bool UI::slider(float x, float y, float w, float& value, float minv, float maxv, const std::string& fmt, const InputState& in) {
    float h = 10.f;
    fillRect(x, y, w, h, vec4{0.12f, 0.14f, 0.18f, 1.f});
    float t = (value - minv) / (maxv - minv);
    t = std::max(0.f, std::min(1.f, t));
    fillRect(x, y, w * t, h, vec4{0.30f, 0.60f, 0.85f, 1.f});
    float knobX = x + w * t;
    fillRect(knobX - 5.f, y - 2.f, 10.f, h + 4.f, vec4{0.85f, 0.9f, 0.98f, 1.f});

    char buf[32];
    snprintf(buf, sizeof(buf), fmt.c_str(), value);
    drawText(x + w + 6.f, y - 2.f, 1.3f, vec4{0.85f, 0.85f, 0.9f, 1.f}, buf);

    if (pointInRect(in, x, y, w, h) && in.mouseDown[0]) {
        float nt = (in.mouseX - x) / w;
        value = minv + (maxv - minv) * std::max(0.f, std::min(1.f, nt));
        return true;
    }
    return false;
}

bool UI::textInput(float x, float y, float w, float h, std::string& text, const InputState& in) {
    int id = m_nextId++;
    fillRect(x, y, w, h, vec4{0.08f, 0.10f, 0.14f, 1.f});
    bool hover = pointInRect(in, x, y, w, h);
    if (hover && in.mouseClicked[0]) { m_focused = true; m_focusId = id; }
    if (!m_focused || m_focusId != id) return hover;

    if (in.pressed(Key::Backspace) && !text.empty()) text.pop_back();
    if (in.typedChar) text.push_back(in.typedChar);
    if (in.pressed(Key::Enter)) { m_focused = false; }

    drawText(x + 4.f, y + (h - FONT_GLYPH_H * 1.4f) / 2.f, 1.4f, vec4{0.92f, 0.95f, 0.98f, 1.f}, text);
    // cursor
    float cw = textWidth(text, 1.4f);
    fillRect(x + 4.f + cw, y + 2.f, 2.f, h - 4.f, vec4{0.6f, 0.8f, 1.f, 1.f});
    return true;
}

bool UI::textEditor(float x, float y, float w, float h, std::string& text, const InputState& in) {
    int id = m_nextId++;
    fillRect(x, y, w, h, vec4{0.07f, 0.09f, 0.13f, 1.f});
    bool hover = pointInRect(in, x, y, w, h);
    if (hover && in.mouseClicked[0]) { m_focused = true; m_focusId = id; }
    if (!m_focused || m_focusId != id) return hover;

    // editing (cursor at end for simplicity; Enter inserts newline)
    if (in.pressed(Key::Backspace) && !text.empty()) {
        size_t pos = text.find_last_of('\n') == std::string::npos ? text.size() : text.size();
        if (text.back() == '\n') text.pop_back();
        else text.pop_back();
        (void)pos;
    }
    if (in.pressed(Key::Enter)) text.push_back('\n');
    if (in.typedChar) text.push_back(in.typedChar);

    // render with basic scroll (keep it simple: draw from bottom)
    float lineH = FONT_GLYPH_H * 1.6f * 1.5f;
    float startY = y + 4.f;
    // count lines from the end
    int lines = 1;
    for (char c : text) if (c == '\n') ++lines;
    int maxLines = (int)(h / lineH);
    int skip = lines > maxLines ? lines - maxLines : 0;

    std::string line;
    int lineIdx = 0;
    size_t i = 0;
    while (i <= text.size()) {
        size_t nl = text.find('\n', i);
        if (nl == std::string::npos) line = text.substr(i);
        else line = text.substr(i, nl - i);
        if (lineIdx >= skip) {
            drawText(x + 4.f, startY + (lineIdx - skip) * lineH, 1.4f,
                     vec4{0.92f, 0.95f, 0.98f, 1.f}, line);
        }
        ++lineIdx;
        if (nl == std::string::npos) break;
        i = nl + 1;
    }
    // cursor at end of visible text
    float curX = x + 4.f + textWidth(text.substr(text.rfind('\n') == std::string::npos ? 0 : text.rfind('\n') + 1), 1.4f);
    float curY = startY + (std::max(0, lines - 1 - skip)) * lineH;
    if (curY < y + h - lineH)
        fillRect(curX, curY, 2.f, lineH * 0.9f, vec4{0.6f, 0.8f, 1.f, 1.f});
    return true;
}

bool UI::window(float x, float y, float w, float h, const std::string& title,
                const InputState& in, float* outX, float* outY, float* outW, float* outH) {
    if (m_dragging) {
        x = m_winX;
        y = m_winY;
    } else {
        m_winX = x;
        m_winY = y;
    }
    fillRect(x, y, w, h, vec4{0.10f, 0.12f, 0.16f, 0.92f});
    fillRect(x, y, w, 20.f, vec4{0.18f, 0.20f, 0.26f, 1.f});
    drawText(x + 6.f, y + 4.f, 1.4f, vec4{0.9f, 0.92f, 0.96f, 1.f}, title);

    // drag
    if (pointInRect(in, x, y, w, 20.f) && in.mouseDown[0]) {
        if (!m_dragging) {
            m_dragging = true;
            m_dragOffX = in.mouseX - x;
            m_dragOffY = in.mouseY - y;
        }
        m_winX = in.mouseX - m_dragOffX;
        m_winY = in.mouseY - m_dragOffY;
    }
    if (m_dragging && !in.mouseDown[0]) m_dragging = false;

    if (outX) *outX = x;
    if (outY) *outY = y + 20.f;
    if (outW) *outW = w;
    if (outH) *outH = h - 20.f;
    return true;
}

} // namespace rb

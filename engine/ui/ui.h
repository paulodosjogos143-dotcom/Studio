// ROXBOX - immediate-mode UI (drawn on top of the 3D viewport)
#pragma once
#include "../render/gl_backend.h"
#include "../input/input.h"
#include <string>
#include <vector>

namespace rb {

class UI {
public:
    explicit UI(GLBackend& backend) : m_backend(backend) {}

    bool init(GLBackend& backend, int width, int height);
    void beginFrame(int width, int height, const InputState& in);
    void endFrame();

    // primitives (screen pixels)
    void fillRect(float x, float y, float w, float h, vec4 color);
    void drawText(float x, float y, float scale, vec4 color, const std::string& s);
    float textWidth(const std::string& s, float scale) const;

    // widgets
    bool button(float x, float y, float w, float h, const std::string& label, const InputState& in);
    bool checkbox(float x, float y, const std::string& label, bool& state, const InputState& in);
    bool slider(float x, float y, float w, float& value, float minv, float maxv, const std::string& fmt, const InputState& in);
    bool textInput(float x, float y, float w, float h, std::string& text, const InputState& in);
    bool textEditor(float x, float y, float w, float h, std::string& text, const InputState& in);

    // draggable window; returns content rect via out params (false = skip)
    bool window(float x, float y, float w, float h, const std::string& title,
                const InputState& in, float* outX, float* outY, float* outW, float* outH);

    bool inputFocused() const { return m_focused; }
    void clearFocus() { m_focused = false; }

private:
    struct Quad {
        float x, y, w, h;
        float u0, v0, u1, v1;
        float r, g, b, a;
        bool text;
    };

    GLBackend& m_backend;
    int m_width = 0, m_height = 0;
    ProgramHandle m_program = 0;
    TextureHandle m_font = 0;
    std::vector<Quad> m_quads;
    const InputState* m_in = nullptr;

    // window drag state
    bool m_dragging = false;
    float m_dragOffX = 0.f, m_dragOffY = 0.f;
    float m_winX = 0.f, m_winY = 0.f;

    // text input focus
    bool m_focused = false;
    int m_focusId = -1;
    int m_nextId = 0;

    void addQuad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                 float r, float g, float b, float a, bool text);
};

} // namespace rb

// ROXBOX - GL backend interface (shared by the unified gl_backend.cpp)
// One implementation covers: OpenGL ES 2.0 (Android, ultra-low-end),
// desktop OpenGL 3.3 core (PC, max quality), and a headless stub (tests).
#pragma once
#include "../core/math.h"
#include <cstdint>
#include <string>

namespace rb {

// Opaque handles: 0 = invalid
using TextureHandle = int;
using MeshHandle = int;
using ProgramHandle = int;

struct BackendStats {
    int drawCalls = 0;
    int triangles = 0;
    bool glError = false;
};

// vertex layout shared by the mesher and the backend:
// pos(3) uv(2) shade(1) tint(1) wave(1) emi(1)
inline constexpr int BACKEND_FLOATS_PER_VERT = 9;

class GLBackend {
public:
    // context must already exist; sets up viewport + global state
    bool init(int width, int height);
    void resize(int width, int height);

    void clear(float r, float g, float b);

    // texture from RGBA8 pixels
    TextureHandle createTexture(int w, int h, const uint8_t* rgba);
    void destroyTexture(TextureHandle h);
    void bindTexture(int unit, TextureHandle h);

    ProgramHandle createProgram(const char* vsSource, const char* fsSource, std::string& err);
    void useProgram(ProgramHandle h);
    void setUniform1f(const char* name, float v);
    void setUniform1i(const char* name, int v);
    void setUniform2f(const char* name, float x, float y);
    void setUniform3f(const char* name, float x, float y, float z);
    void setUniformMat4(const char* name, const float* m16);

    MeshHandle createMesh(const float* verts, size_t vertCount, const uint32_t* indices, size_t indexCount);
    void destroyMesh(MeshHandle h);
    void drawMesh(MeshHandle h);

    // simple point particles (GL_POINTS): posSize = (x,y,z,size) per point,
    // colorAlpha = (r,g,b,a) per point
    void drawPoints(const float* posSize, const float* colorAlpha, int count);

    // global render state
    void setBlend(bool transparent);
    void setAdditive(bool on);
    void setDepthWrite(bool on);

    int lastError();
    BackendStats& stats() { return m_stats; }

private:
    int m_width = 0, m_height = 0;
    BackendStats m_stats;
};

// shader sources for the active backend (GLSL ES 1.00 or GLSL 3.30)
const char* backendVertexShader();
const char* backendFragmentShader();

} // namespace rb

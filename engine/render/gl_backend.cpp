// ROXBOX - unified GL backend
// Compiled for three configurations:
//   ROXBOX_GLES2   -> OpenGL ES 2.0 (Android / ultra-low-end, GLSL ES 1.00)
//   ROXBOX_GL3     -> desktop OpenGL 3.3 core (PC, GLSL 330)
//   ROXBOX_HEADLESS-> stub GL (verification builds, no driver needed)
#include "gl_backend.h"
#include "../core/log.h"
#include <string>
#include <vector>
#include <cstring>
#include <cmath>

#if defined(ROXBOX_HEADLESS)
#  include "gl_stub/gl.h"
#elif defined(ROXBOX_GLES2)
#  include <GLES2/gl2.h>
#else
#  define ROXBOX_GL3
#  include <GL/gl.h>
#endif

namespace rb {

namespace {

struct MeshRec {
    GLuint vbo = 0, ibo = 0;
    size_t vcount = 0, icount = 0;
};
struct TextureRec {
    GLuint tex = 0;
    int w = 0, h = 0;
};
struct ProgRec {
    GLuint prog = 0;
    struct UL { uint32_t hash; GLint loc; } ul[24];
    int ulCount = 0;
    GLint loc(const char* name) {
        uint32_t h = 2166136261u;
        for (const char* p = name; *p; ++p) { h ^= (uint32_t)(unsigned char)*p; h *= 16777619u; }
        for (int i = 0; i < ulCount; ++i) if (ul[i].hash == h) return ul[i].loc;
        if (ulCount < 24) { ul[ulCount].hash = h; ul[ulCount].loc = glGetUniformLocation(prog, name); ++ulCount; }
        return ul[ulCount - 1].loc;
    }
};

std::vector<MeshRec>& meshes() { static std::vector<MeshRec> v; return v; }
std::vector<TextureRec>& textures() { static std::vector<TextureRec> v; return v; }
std::vector<ProgRec>& programs() { static std::vector<ProgRec> v; return v; }

} // namespace

const char* backendVertexShader() {
#if defined(ROXBOX_GLES2) || defined(ROXBOX_HEADLESS)
    return R"GLSL(
attribute vec3 aPos;
attribute vec2 aUV;
attribute float aShade;
attribute float aTint;
attribute float aWave;
attribute float aEmi;
uniform mat4 uProj;
uniform mat4 uView;
uniform vec3 uCamPos;
uniform float uTime;
uniform float uFogNear;
uniform float uFogFar;
varying vec2 vUV;
varying float vShade;
varying float vTint;
varying float vEmi;
varying float vFog;
void main() {
    vec2 uv = aUV;
    if (aWave > 0.5) {
        uv.y += sin(uTime * 2.0 + aPos.x * 0.6 + aPos.z * 0.4) * 0.015;
    }
    vec4 vp = uView * vec4(aPos, 1.0);
    gl_Position = uProj * vp;
    float d = length(aPos - uCamPos);
    vFog = clamp((d - uFogNear) / max(uFogFar - uFogNear, 0.001), 0.0, 1.0);
    vUV = uv;
    vShade = aShade;
    vTint = aTint;
    vEmi = aEmi;
}
)GLSL";
#else
    return R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aShade;
layout(location = 3) in float aTint;
layout(location = 4) in float aWave;
layout(location = 5) in float aEmi;
uniform mat4 uProj;
uniform mat4 uView;
uniform vec3 uCamPos;
uniform float uTime;
uniform float uFogNear;
uniform float uFogFar;
out vec2 vUV;
out float vShade;
out float vTint;
out float vEmi;
out float vFog;
void main() {
    vec2 uv = aUV;
    if (aWave > 0.5) {
        uv.y += sin(uTime * 2.0 + aPos.x * 0.6 + aPos.z * 0.4) * 0.015;
    }
    vec4 vp = uView * vec4(aPos, 1.0);
    gl_Position = uProj * vp;
    float d = length(aPos - uCamPos);
    vFog = clamp((d - uFogNear) / max(uFogFar - uFogNear, 0.001), 0.0, 1.0);
    vUV = uv;
    vShade = aShade;
    vTint = aTint;
    vEmi = aEmi;
}
)GLSL";
#endif
}

const char* backendFragmentShader() {
#if defined(ROXBOX_GLES2) || defined(ROXBOX_HEADLESS)
    return R"GLSL(
precision mediump float;
uniform sampler2D uAtlas;
uniform sampler2D uPalette;
uniform vec3 uFogColor;
varying vec2 vUV;
varying float vShade;
varying float vTint;
varying float vEmi;
varying float vFog;
void main() {
    vec4 tex = texture2D(uAtlas, vUV);
    vec3 tint = texture2D(uPalette, vec2((vTint + 0.5) / 24.0, 0.5)).rgb;
    vec3 col = tex.rgb * tint * vShade;
    if (vEmi > 0.5) {
        col = tex.rgb * tint * 1.2;
    }
    col = mix(col, uFogColor, vFog);
    gl_FragColor = vec4(col, tex.a);
}
)GLSL";
#else
    return R"GLSL(
#version 330 core
precision highp float;
uniform sampler2D uAtlas;
uniform sampler2D uPalette;
uniform vec3 uFogColor;
in vec2 vUV;
in float vShade;
in float vTint;
in float vEmi;
in float vFog;
out vec4 fragColor;
void main() {
    vec4 tex = texture(uAtlas, vUV);
    vec3 tint = texture(uPalette, vec2((vTint + 0.5) / 24.0, 0.5)).rgb;
    vec3 col = tex.rgb * tint * vShade;
    if (vEmi > 0.5) {
        col = tex.rgb * tint * 1.2;
    }
    col = mix(col, uFogColor, vFog);
    fragColor = vec4(col, tex.a);
}
)GLSL";
#endif
}

bool GLBackend::init(int width, int height) {
    m_width = width; m_height = height;
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.53f, 0.70f, 0.95f, 1.f);
    return true;
}

void GLBackend::resize(int width, int height) {
    m_width = width; m_height = height;
    glViewport(0, 0, width, height);
}

void GLBackend::clear(float r, float g, float b) {
    glClearColor(r, g, b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

TextureHandle GLBackend::createTexture(int w, int h, const uint8_t* rgba) {
    if (w <= 0 || h <= 0 || !rgba) return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    textures().push_back({tex, w, h});
    return (TextureHandle)textures().size();
}

void GLBackend::destroyTexture(TextureHandle h) {
    if (h <= 0 || h > (TextureHandle)textures().size()) return;
    GLuint tex = textures()[h - 1].tex;
    glDeleteTextures(1, &tex);
    textures()[h - 1] = textures().back();
    textures().pop_back();
}

void GLBackend::bindTexture(int unit, TextureHandle h) {
    GLuint tex = 0;
    if (h > 0 && h <= (TextureHandle)textures().size()) tex = textures()[h - 1].tex;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex);
}

ProgramHandle GLBackend::createProgram(const char* vsSource, const char* fsSource, std::string& err) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, (const GLchar* const*)&vsSource, nullptr);
    glCompileShader(vs);
    GLint ok = GL_FALSE;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(vs, 1024, nullptr, log);
        err = std::string("VS compile error: ") + log;
        glDeleteShader(vs);
        return 0;
    }
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, (const GLchar* const*)&fsSource, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(fs, 1024, nullptr, log);
        err = std::string("FS compile error: ") + log;
        glDeleteShader(vs); glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        err = std::string("link error: ") + log;
        glDeleteShader(vs); glDeleteShader(fs); glDeleteProgram(prog);
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    programs().push_back({prog});
    return (ProgramHandle)programs().size();
}

void GLBackend::useProgram(ProgramHandle h) {
    if (h > 0 && h <= (ProgramHandle)programs().size())
        glUseProgram(programs()[h - 1].prog);
}

void GLBackend::setUniform1f(const char* name, float v) {
    if (programs().empty()) return;
    glUniform1f(programs().back().loc(name), v);
}
void GLBackend::setUniform1i(const char* name, int v) {
    if (programs().empty()) return;
    glUniform1i(programs().back().loc(name), v);
}
void GLBackend::setUniform2f(const char* name, float x, float y) {
    if (programs().empty()) return;
    glUniform2f(programs().back().loc(name), x, y);
}
void GLBackend::setUniform3f(const char* name, float x, float y, float z) {
    if (programs().empty()) return;
    glUniform3f(programs().back().loc(name), x, y, z);
}
void GLBackend::setUniformMat4(const char* name, const float* m16) {
    if (programs().empty()) return;
    glUniformMatrix4fv(programs().back().loc(name), 1, GL_FALSE, m16);
}

MeshHandle GLBackend::createMesh(const float* verts, size_t vertCount, const uint32_t* indices, size_t indexCount) {
    if (!verts || vertCount == 0) return 0;
    GLuint vbo = 0, ibo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertCount * BACKEND_FLOATS_PER_VERT * sizeof(float)), verts, GL_STATIC_DRAW);
    if (indices && indexCount > 0) {
        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indexCount * sizeof(uint32_t)), indices, GL_STATIC_DRAW);
    }
    meshes().push_back({vbo, ibo, vertCount, indexCount});
    return (MeshHandle)meshes().size();
}

void GLBackend::destroyMesh(MeshHandle h) {
    if (h <= 0 || h > (MeshHandle)meshes().size()) return;
    GLuint vbo = meshes()[h - 1].vbo, ibo = meshes()[h - 1].ibo;
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ibo) glDeleteBuffers(1, &ibo);
    meshes()[h - 1] = meshes().back();
    meshes().pop_back();
}

void GLBackend::drawMesh(MeshHandle h) {
    if (h <= 0 || h > (MeshHandle)meshes().size()) return;
    const MeshRec& m = meshes()[h - 1];
    if (m.vcount == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    const GLsizei stride = BACKEND_FLOATS_PER_VERT * sizeof(float);
    // aPos, aUV, aShade, aTint, aWave, aEmi
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(5);
    if (m.ibo) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
        glDrawElements(GL_TRIANGLES, (GLsizei)m.icount, GL_UNSIGNED_INT, 0);
    } else {
        glDrawElements(GL_TRIANGLES, (GLsizei)m.vcount, GL_UNSIGNED_INT, 0);
    }
    m_stats.drawCalls++;
    m_stats.triangles += (int)(m.icount / 3);
    if (glGetError() != 0) m_stats.glError = true;
}

void GLBackend::setBlend(bool transparent) {
    if (transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void GLBackend::setDepthWrite(bool on) {
    glDepthMask(on ? GL_TRUE : GL_FALSE);
}

int GLBackend::lastError() {
    return (int)glGetError();
}

// ---------------- point particles ----------------

namespace {
const char* pointVertexShader() {
#if defined(ROXBOX_GLES2) || defined(ROXBOX_HEADLESS)
    return R"GLSL(
attribute vec3 aPos;
attribute float aSize;
attribute vec4 aColor;
uniform mat4 uProj;
uniform mat4 uView;
varying vec4 vColor;
void main() {
    gl_Position = uProj * uView * vec4(aPos, 1.0);
    gl_PointSize = aSize;
    vColor = aColor;
}
)GLSL";
#else
    return R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec4 aColor;
uniform mat4 uProj;
uniform mat4 uView;
out vec4 vColor;
void main() {
    gl_Position = uProj * uView * vec4(aPos, 1.0);
    gl_PointSize = aSize;
    vColor = aColor;
}
)GLSL";
#endif
}
const char* pointFragmentShader() {
#if defined(ROXBOX_GLES2) || defined(ROXBOX_HEADLESS)
    return R"GLSL(
precision mediump float;
varying vec4 vColor;
void main() {
    gl_FragColor = vColor;
}
)GLSL";
#else
    return R"GLSL(
#version 330 core
precision highp float;
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)GLSL";
#endif
}
} // namespace

void GLBackend::setAdditive(bool on) {
    if (on) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    } else {
        glDisable(GL_BLEND);
    }
}

void GLBackend::drawPoints(const float* posSize, const float* colorAlpha, int count) {
    if (!posSize || count <= 0) return;
    // lazily build the point program
    static ProgramHandle pointProg = 0;
    if (!pointProg) {
        std::string err;
        pointProg = createProgram(pointVertexShader(), pointFragmentShader(), err);
        if (!pointProg) return;
    }
    useProgram(pointProg);
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // layout: pos(3)+size(1); color uploaded in a second VBO
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * 4 * sizeof(float)), posSize, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    GLuint vbo2 = 0;
    glGenBuffers(1, &vbo2);
    glBindBuffer(GL_ARRAY_BUFFER, vbo2);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * 4 * sizeof(float)), colorAlpha, GL_STREAM_DRAW);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*)0);
    glEnableVertexAttribArray(2);
    glDrawArrays(GL_POINTS, 0, count);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &vbo2);
    m_stats.drawCalls++;
    m_stats.triangles += count;
}

} // namespace rb

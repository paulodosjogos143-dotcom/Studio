// ROXBOX - core math (header-only, C++17)
// Minimal, allocation-free, SIMD-friendly.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace rb {

struct vec2 { float x, y; };
struct vec3 {
    float x, y, z;
    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }
};
struct vec4 { float x, y, z, w; };
struct quat { float x, y, z, w; };

inline vec3 operator+(const vec3& a, const vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline vec3 operator-(const vec3& a, const vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline vec3 operator*(const vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline vec3 operator*(float s, const vec3& a) { return a * s; }
inline vec3 operator/(const vec3& a, float s) { return {a.x / s, a.y / s, a.z / s}; }
inline vec3 operator-(const vec3& a) { return {-a.x, -a.y, -a.z}; }

inline float dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 cross(const vec3& a, const vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const vec3& a) { return std::sqrt(dot(a, a)); }
inline vec3 normalize(const vec3& a) { float l = length(a); return l > 1e-8f ? a / l : a; }
inline float distance(const vec3& a, const vec3& b) { return length(a - b); }

inline bool between(float v, float lo, float hi) { return v >= lo && v <= hi; }
inline float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline vec3 mix(const vec3& a, const vec3& b, float t) { return {lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t)}; }

// axis-aligned bounding box
struct aabb {
    vec3 minv{0, 0, 0}, maxv{0, 0, 0};
    bool empty = true;
    static aabb make(const vec3& a, const vec3& b) {
        aabb r; r.minv = {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
        r.maxv = {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
        r.empty = false;
        return r;
    }
    bool contains(const vec3& p) const {
        return between(p.x, minv.x, maxv.x) && between(p.y, minv.y, maxv.y) && between(p.z, minv.z, maxv.z);
    }
};

// column-major 4x4 matrix (OpenGL convention)
struct mat4 {
    float m[16];
    mat4() { identity(); }
    void identity() {
        for (int i = 0; i < 16; ++i) m[i] = 0.f;
        m[0] = m[5] = m[10] = m[15] = 1.f;
    }
    static mat4 perspective(float fovyRad, float aspect, float znear, float zfar) {
        mat4 r;
        float f = 1.f / std::tan(fovyRad * 0.5f);
        r.m[0] = f / aspect; r.m[5] = f;
        r.m[10] = (zfar + znear) / (znear - zfar); r.m[11] = -1.f;
        r.m[14] = (2.f * zfar * znear) / (znear - zfar);
        return r;
    }
    static mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) {
        vec3 f = normalize(center - eye);
        vec3 s = normalize(cross(f, up));
        vec3 u = cross(s, f);
        mat4 r;
        r.m[0] = s.x; r.m[1] = u.x; r.m[2] = f.x; r.m[3] = 0;
        r.m[4] = s.y; r.m[5] = u.y; r.m[6] = f.y; r.m[7] = 0;
        r.m[8] = s.z; r.m[9] = u.z; r.m[10] = f.z; r.m[11] = 0;
        r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = -dot(f, eye); r.m[15] = 1;
        return r;
    }
    static mat4 translation(const vec3& t) {
        mat4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r;
    }
    mat4 operator*(const mat4& o) const {
        mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + row] * o.m[c * 4 + k];
                r.m[c * 4 + row] = s;
            }
        return r;
    }
    vec3 transformPoint(const vec3& p) const {
        float x = m[0]*p.x + m[4]*p.y + m[8]*p.z + m[12];
        float y = m[1]*p.x + m[5]*p.y + m[9]*p.z + m[13];
        float z = m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14];
        float w = m[3]*p.x + m[7]*p.y + m[11]*p.z + m[15];
        return w != 0.f ? vec3{x / w, y / w, z / w} : vec3{x, y, z};
    }

    // full 4x4 inverse (for unprojecting screen coordinates)
    mat4 inverse() const {
        float a00 = m[0], a01 = m[1], a02 = m[2], a03 = m[3];
        float a10 = m[4], a11 = m[5], a12 = m[6], a13 = m[7];
        float a20 = m[8], a21 = m[9], a22 = m[10], a23 = m[11];
        float a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15];
        float b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10;
        float b02 = a00 * a13 - a03 * a10, b03 = a01 * a12 - a02 * a11;
        float b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12;
        float b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30;
        float b08 = a20 * a33 - a23 * a30, b09 = a21 * a32 - a22 * a31;
        float b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;
        float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
        mat4 r;
        if (std::abs(det) < 1e-12f) return r;
        det = 1.f / det;
        r.m[0] = (a11 * b11 - a12 * b10 + a13 * b09) * det;
        r.m[1] = (a02 * b10 - a01 * b11 - a03 * b09) * det;
        r.m[2] = (a31 * b05 - a32 * b04 + a33 * b03) * det;
        r.m[3] = (a22 * b04 - a21 * b05 - a23 * b03) * det;
        r.m[4] = (a12 * b08 - a10 * b11 - a13 * b07) * det;
        r.m[5] = (a00 * b11 - a02 * b08 + a03 * b07) * det;
        r.m[6] = (a32 * b02 - a30 * b05 - a33 * b01) * det;
        r.m[7] = (a20 * b05 - a22 * b02 + a23 * b01) * det;
        r.m[8] = (a10 * b10 - a11 * b08 + a13 * b06) * det;
        r.m[9] = (a01 * b08 - a00 * b10 - a03 * b06) * det;
        r.m[10] = (a30 * b04 - a31 * b02 + a33 * b00) * det;
        r.m[11] = (a21 * b02 - a20 * b04 - a23 * b00) * det;
        r.m[12] = (a11 * b07 - a10 * b09 - a12 * b06) * det;
        r.m[13] = (a00 * b09 - a01 * b07 + a02 * b06) * det;
        r.m[14] = (a31 * b01 - a30 * b03 - a32 * b00) * det;
        r.m[15] = (a20 * b03 - a21 * b01 + a22 * b00) * det;
        return r;
    }
};

// deterministic xorshift rng (no allocation, tiny)
struct rng {
    uint32_t s;
    explicit rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
    uint32_t next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    float range(float a, float b) { return a + (b - a) * (float)((next() >> 8) * (1.0 / 16777216.0)); }
    int range(int a, int b) { return a + (int)(next() % (uint32_t)(b - a + 1)); }
};

// 3D value noise (deterministic, seeded) for terrain generation
float valueNoise3(const vec3& p, uint32_t seed);

inline float smoothNoise3(const vec3& p, uint32_t seed) {
    int ix = (int)std::floor(p.x), iy = (int)std::floor(p.y), iz = (int)std::floor(p.z);
    float fx = p.x - ix, fy = p.y - iy, fz = p.z - iz;
    float ux = fx * fx * (3.f - 2.f * fx), uy = fy * fy * (3.f - 2.f * fy), uz = fz * fz * (3.f - 2.f * fz);
    float v000 = valueNoise3({(float)ix, (float)iy, (float)iz}, seed);
    float v100 = valueNoise3({(float)ix + 1, (float)iy, (float)iz}, seed);
    float v010 = valueNoise3({(float)ix, (float)iy + 1, (float)iz}, seed);
    float v110 = valueNoise3({(float)ix + 1, (float)iy + 1, (float)iz}, seed);
    float v001 = valueNoise3({(float)ix, (float)iy, (float)iz + 1}, seed);
    float v101 = valueNoise3({(float)ix + 1, (float)iy, (float)iz + 1}, seed);
    float v011 = valueNoise3({(float)ix, (float)iy + 1, (float)iz + 1}, seed);
    float v111 = valueNoise3({(float)ix + 1, (float)iy + 1, (float)iz + 1}, seed);
    float x00 = lerp(v000, v100, ux), x10 = lerp(v010, v110, ux);
    float x01 = lerp(v001, v101, ux), x11 = lerp(v011, v111, ux);
    return lerp(lerp(x00, x10, uy), lerp(x01, x11, uy), uz);
}

inline float fbm3(const vec3& p, uint32_t seed, int octaves) {
    float sum = 0, amp = 1, freq = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += smoothNoise3(p * freq, seed + i * 7919u) * amp;
        norm += amp; amp *= 0.5f; freq *= 2.f;
    }
    return sum / norm;
}

} // namespace rb

#include "camera.h"
#include <cmath>

namespace rb {

void Camera::orbit(float dx, float dy, float zoomFactor) {
    yaw -= dx * 0.005f;
    pitch = clamp(pitch - dy * 0.005f, -1.5f, 1.5f);
    distance = clamp(distance * zoomFactor, 2.f, 300.f);
}

vec3 Camera::forward() const {
    float cp = std::cos(pitch), sp = std::sin(pitch);
    float cy = std::cos(yaw), sy = std::sin(yaw);
    return {-sy * cp, sp, -cy * cp};
}

vec3 Camera::right() const {
    float cy = std::cos(yaw), sy = std::sin(yaw);
    return {cy, 0.f, -sy};
}

void Camera::fly(float fwd, float strafe, float up) {
    vec3 f = forward();
    vec3 r = right();
    vec3 horiz = normalize(vec3{f.x, 0.f, f.z});
    float speed = distance < 12.f ? 6.f : 22.f;
    position = position + horiz * (fwd * speed) + r * (strafe * speed) + vec3{0, 1, 0} * (up * speed);
}

mat4 Camera::viewMatrix() const {
    vec3 dir = forward() * distance;
    vec3 eye = target + dir;
    return mat4::lookAt(eye, target, vec3{0, 1, 0});
}

mat4 Camera::projMatrix(float aspect) const {
    return mat4::perspective(fov, aspect, nearPlane, farPlane);
}

// ---------------- frustum ----------------

Frustum makeFrustum(const mat4& m) {
    Frustum f;
    // column-major matrix: row i element j lives at m[i + 4*j]
    // planes: right=row0-row3, left=row0+row3, top=row1-row3, bottom=row1+row3,
    //         far=row2-row3, near=row2+row3  (normals point inward)
    float row[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            row[i][j] = m.m[i + 4 * j];
    int combos[6][2] = { {0,-1}, {0, 1}, {1,-1}, {1, 1}, {2,-1}, {2, 1} };
    for (int i = 0; i < 6; ++i) {
        // plane = row[idx] + sign*row3  (sign=-1: right/top/far, +1: left/bottom/near)
        float a = row[combos[i][0]][0] + combos[i][1] * row[3][0];
        float b = row[combos[i][0]][1] + combos[i][1] * row[3][1];
        float c = row[combos[i][0]][2] + combos[i][1] * row[3][2];
        float d = row[combos[i][0]][3] + combos[i][1] * row[3][3];
        float len = std::sqrt(a * a + b * b + c * c);
        f.planes[i] = {a / len, b / len, c / len, d / len};
    }
    return f;
}

bool Frustum::intersectsAABB(const aabb& box) const {
    vec3 minv = box.minv, maxv = box.maxv;
    for (int i = 0; i < 6; ++i) {
        const vec4& p = planes[i];
        vec3 vertex = { (p.x > 0 ? maxv.x : minv.x), (p.y > 0 ? maxv.y : minv.y), (p.z > 0 ? maxv.z : minv.z) };
        if (p.x * vertex.x + p.y * vertex.y + p.z * vertex.z + p.w < 0.f) return false;
    }
    return true;
}

} // namespace rb

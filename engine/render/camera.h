// ROXBOX - orbit/fly camera + frustum culling
#pragma once
#include "../core/math.h"

namespace rb {

struct Frustum {
    vec4 planes[6]; // a,b,c,d : ax+by+cz+d=0, positive inside
    bool intersectsAABB(const aabb& box) const;
};

// extracts frustum planes from a view-projection matrix
Frustum makeFrustum(const mat4& vp);

class Camera {
public:
    vec3 position{0, 20, 30};
    vec3 target{0, 0, 0};
    float yaw = 0.5f, pitch = -0.35f;
    float distance = 40.f;
    float fov = 60.f * 3.14159265f / 180.f;
    float nearPlane = 0.05f;
    float farPlane = 400.f;

    // orbit mode: rotate around target by dx/dy, zoom by dy-scroll
    void orbit(float dx, float dy, float zoomFactor);
    // fly: move along camera-relative axes
    void fly(float forward, float strafe, float up);

    vec3 forward() const;
    vec3 right() const;
    mat4 viewMatrix() const;
    mat4 projMatrix(float aspect) const;
    Frustum frustum(float aspect) const { return makeFrustum(projMatrix(aspect) * viewMatrix()); }
};

} // namespace rb

// ROXBOX - shared character controller (voxel AABB collision)
#pragma once
#include "../world/voxelworld.h"
#include "../core/math.h"
#include <cmath>

namespace rb {

struct Character {
    vec3 pos{0, 0, 0};   // center of the capsule
    vec3 vel{0, 0, 0};
    float radius = 0.32f;
    float height = 1.7f;
    bool grounded = false;

    aabb box() const {
        return aabb::make(pos - vec3{radius, height * 0.5f, radius},
                          pos + vec3{radius, height * 0.5f, radius});
    }
};

inline bool boxHitsVoxels(VoxelWorld& world, const aabb& box) {
    int x0 = (int)std::floor(box.minv.x), y0 = (int)std::floor(box.minv.y), z0 = (int)std::floor(box.minv.z);
    int x1 = (int)std::floor(box.maxv.x), y1 = (int)std::floor(box.maxv.y), z1 = (int)std::floor(box.maxv.z);
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                if (blockIsSolid(world.getBlock(x, y, z))) return true;
    return false;
}

// per-axis swept AABB collision resolution
inline void moveAxis(VoxelWorld& world, Character& c, int axis, float delta) {
    vec3 old = c.pos;
    c.pos[axis] += delta;
    aabb box = c.box();
    if (!boxHitsVoxels(world, box)) return;

    int x0 = (int)std::floor(box.minv.x), y0 = (int)std::floor(box.minv.y), z0 = (int)std::floor(box.minv.z);
    int x1 = (int)std::floor(box.maxv.x), y1 = (int)std::floor(box.maxv.y), z1 = (int)std::floor(box.maxv.z);
    float bestPen = 1e9f;
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z) {
                if (!blockIsSolid(world.getBlock(x, y, z))) continue;
                float pen;
                if (axis == 0) pen = (delta > 0) ? (box.maxv.x - (float)x) : ((float)(x + 1) - box.minv.x);
                else if (axis == 1) pen = (delta > 0) ? (box.maxv.y - (float)y) : ((float)(y + 1) - box.minv.y);
                else pen = (delta > 0) ? (box.maxv.z - (float)z) : ((float)(z + 1) - box.minv.z);
                if (pen < bestPen) bestPen = pen;
            }
    c.pos[axis] = old[axis] + delta - bestPen;
    if (axis == 1 && delta < 0) c.grounded = true;
    c.vel[axis] = 0.f;
}

inline vec3 findGround(VoxelWorld& world, vec3 start) {
    vec3 p = start;
    for (int i = 0; i < 128; ++i) {
        int yi = (int)std::floor(p.y);
        if (blockIsSolid(world.getBlock((int)p.x, yi, (int)p.z))) {
            p.y = (float)(yi + 1) + 0.1f;
            break;
        }
        p.y -= 1.f;
    }
    return p;
}

// voxel raycast (Amanatides & Woo DDA)
struct RayHit {
    bool hit = false;
    vec3 voxel{0, 0, 0}; // integer voxel coords of the hit
    vec3 normal{0, 0, 0};
    float dist = 0.f;
};

inline RayHit raycastVoxel(VoxelWorld& world, const vec3& origin, const vec3& dir, float maxDist = 200.f) {
    RayHit r;
    vec3 d = normalize(dir);
    vec3 i = {std::floor(origin.x), std::floor(origin.y), std::floor(origin.z)};
    vec3 step = {(int)std::signbit(d.x) ? -1 : 1, (int)std::signbit(d.y) ? -1 : 1, (int)std::signbit(d.z) ? -1 : 1};
    vec3 tMax = {(step.x > 0 ? (i.x + 1.f - origin.x) : (origin.x - i.x)) / std::abs(d.x),
                 (step.y > 0 ? (i.y + 1.f - origin.y) : (origin.y - i.y)) / std::abs(d.y),
                 (step.z > 0 ? (i.z + 1.f - origin.z) : (origin.z - i.z)) / std::abs(d.z)};

    float t = 0.f;
    vec3 n{0, 0, 0};
    while (t <= maxDist) {
        uint8_t b = world.getBlock((int)i.x, (int)i.y, (int)i.z);
        if (b != BLK_AIR) {
            r.hit = true;
            r.voxel = i;
            r.normal = n;
            r.dist = t;
            return r;
        }
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            i.x += step.x; t = tMax.x; tMax.x += std::abs(1.f / d.x); n = {-step.x, 0, 0};
        } else if (tMax.y < tMax.z) {
            i.y += step.y; t = tMax.y; tMax.y += std::abs(1.f / d.y); n = {0, -step.y, 0};
        } else {
            i.z += step.z; t = tMax.z; tMax.z += std::abs(1.f / d.z); n = {0, 0, -step.z};
        }
    }
    return r;
}

} // namespace rb

// ROXBOX - scene renderer: chunk meshing pipeline, frustum culling,
// opaque/transparent passes, particles and draw stats.
#pragma once
#include "gl_backend.h"
#include "camera.h"
#include "quality.h"
#include "../mesh/mesher.h"
#include "../world/voxelworld.h"
#include <unordered_map>
#include <vector>

namespace rb {

struct Particle {
    vec3 pos, vel;
    float life, maxLife;
    float r, g, b;
    float size;
};

struct RenderStats {
    int visibleChunks = 0;
    int drawCalls = 0;
    int triangles = 0;
    int particles = 0;
};

class Renderer {
public:
    explicit Renderer(GLBackend& backend) : m_backend(backend) {}

    bool init(GLBackend& backend, int width, int height);
    void resize(int width, int height);

    // per-frame update: mesh dirty chunks within budget, cull and draw
    void update(VoxelWorld& world, const Camera& camera, const QualityManager& quality, float timeSec);

    // particle effects (Roblox-style)
    void spawnParticles(const vec3& pos, int count, const vec3& color, float size, float speed, float life);
    void updateParticles(float dt);
    void clearParticles() { m_particles.clear(); }

    RenderStats& stats() { return m_stats; }
    GLBackend& backend() { return m_backend; }

private:
    void meshChunk(VoxelWorld& world, int cx, int cy, int cz);
    void drawWorld(VoxelWorld& world, const Camera& camera, const QualityManager& quality, float timeSec);
    void drawParticles();

    GLBackend& m_backend;
    int m_width = 0, m_height = 0;
    ProgramHandle m_program = 0;
    TextureHandle m_atlas = 0;
    TextureHandle m_palette = 0;

    std::unordered_map<int64_t, MeshHandle> m_opaqueMesh;
    std::unordered_map<int64_t, MeshHandle> m_transparentMesh;

    std::vector<Particle> m_particles;
    RenderStats m_stats;
    float m_timeSec = 0.f;
};

} // namespace rb

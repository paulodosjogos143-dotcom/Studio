#include "renderer.h"
#include "textureatlas.h"
#include "../core/log.h"
#include <cmath>
#include <algorithm>

namespace rb {

bool Renderer::init(GLBackend& backend, int width, int height) {
    (void)backend; // backend is bound in the constructor
    m_width = width;
    m_height = height;

    std::string err;
    m_program = m_backend.createProgram(backendVertexShader(), backendFragmentShader(), err);
    if (!m_program) {
        RB_LOG_E("shader program failed: %s", err.c_str());
        return false;
    }
    std::vector<uint8_t> atlas = generateAtlas();
    m_atlas = m_backend.createTexture(ATLAS_SIZE, ATLAS_SIZE, atlas.data());
    std::vector<uint8_t> palette = generatePaletteTexture();
    m_palette = m_backend.createTexture(PALETTE_SIZE, 1, palette.data());
    if (!m_atlas || !m_palette) {
        RB_LOG_E("failed to create textures");
        return false;
    }
    RB_LOG_I("renderer initialized (%dx%d)", width, height);
    return true;
}

void Renderer::resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_backend.resize(width, height);
}

void Renderer::meshChunk(VoxelWorld& world, int cx, int cy, int cz) {
    int64_t key = VoxelWorld::chunkKey(cx, cy, cz);

    // reuse a cache slot (bounded memory: old meshes destroyed, slots reused)
    ChunkMesh cm;
    buildChunkMesh(world, cx, cy, cz, cm);

    // destroy previous GPU meshes for this chunk
    auto op = m_opaqueMesh.find(key);
    if (op != m_opaqueMesh.end()) { m_backend.destroyMesh(op->second); m_opaqueMesh.erase(op); }
    auto tr = m_transparentMesh.find(key);
    if (tr != m_transparentMesh.end()) { m_backend.destroyMesh(tr->second); m_transparentMesh.erase(tr); }

    if (!cm.opaque.empty()) {
        MeshHandle h = m_backend.createMesh(cm.opaque.verts.data(), cm.opaque.vertexCount(),
                                            cm.opaque.indices.data(), cm.opaque.indices.size());
        if (h) m_opaqueMesh.emplace(key, h);
    }
    if (!cm.transparent.empty()) {
        MeshHandle h = m_backend.createMesh(cm.transparent.verts.data(), cm.transparent.vertexCount(),
                                            cm.transparent.indices.data(), cm.transparent.indices.size());
        if (h) m_transparentMesh.emplace(key, h);
    }
}

void Renderer::update(VoxelWorld& world, const Camera& camera, const QualityManager& quality, float timeSec) {
    m_timeSec = timeSec;
    m_stats = RenderStats{};

    // 1) meshing budget: keep CPU mesh work bounded for weak hardware
    int budget = quality.meshChunksPerFrame();
    for (int i = 0; i < budget; ++i) {
        int cx, cy, cz;
        if (!world.popDirtyChunk(cx, cy, cz)) break;
        Chunk* c = world.getChunk(cx, cy, cz);
        if (!c) continue;
        meshChunk(world, cx, cy, cz);
        c->dirty = false;
    }

    drawWorld(world, camera, quality, timeSec);
}

void Renderer::drawWorld(VoxelWorld& world, const Camera& camera, const QualityManager& quality, float timeSec) {
    m_backend.clear(0.53f, 0.70f, 0.95f);
    m_backend.useProgram(m_program);
    m_backend.bindTexture(0, m_atlas);
    m_backend.bindTexture(1, m_palette);
    m_backend.setUniform1i("uAtlas", 0);
    m_backend.setUniform1i("uPalette", 1);

    float aspect = (float)m_width / (float)(m_height > 0 ? m_height : 1);
    mat4 proj = camera.projMatrix(aspect);
    mat4 view = camera.viewMatrix();
    m_backend.setUniformMat4("uProj", proj.m);
    m_backend.setUniformMat4("uView", view.m);
    m_backend.setUniform3f("uCamPos", camera.position.x, camera.position.y, camera.position.z);
    m_backend.setUniform1f("uTime", timeSec);

    float fogNear = 20.f;
    float fogFar = quality.fogFar();
    m_backend.setUniform1f("uFogNear", fogNear);
    m_backend.setUniform1f("uFogFar", fogFar);
    m_backend.setUniform3f("uFogColor", 0.53f, 0.70f, 0.95f);

    Frustum frust = camera.frustum(aspect);
    int rd = quality.renderDistance();

    // collect visible chunk AABBs
    struct Vis { int64_t key; aabb box; float dist; };
    std::vector<Vis> visible;
    for (const auto& kv : world.chunks()) {
        const Chunk& c = kv.second;
        vec3 minv{(float)(c.cx * CHUNK_SIZE), (float)(c.cy * CHUNK_SIZE), (float)(c.cz * CHUNK_SIZE)};
        aabb box = aabb::make(minv, minv + vec3{(float)CHUNK_SIZE, (float)CHUNK_SIZE, (float)CHUNK_SIZE});
        vec3 center = {minv.x + 8.f, minv.y + 8.f, minv.z + 8.f};
        vec3 dist = center - camera.position;
        float d = std::sqrt(dot(dist, dist));
        if (d > (float)(rd + 1) * CHUNK_SIZE) continue;
        if (!frust.intersectsAABB(box)) continue;
        visible.push_back({VoxelWorld::chunkKey(c.cx, c.cy, c.cz), box, d});
    }
    // back-to-front for the transparent pass
    std::sort(visible.begin(), visible.end(), [](const Vis& a, const Vis& b) {
        return a.dist > b.dist;
    });

    // opaque pass
    m_backend.setBlend(false);
    m_backend.setDepthWrite(true);
    for (const Vis& v : visible) {
        auto it = m_opaqueMesh.find(v.key);
        if (it != m_opaqueMesh.end())
            m_backend.drawMesh(it->second);
    }

    // transparent pass (glass / water): blend, no depth writes
    m_backend.setBlend(true);
    m_backend.setDepthWrite(false);
    for (const Vis& v : visible) {
        auto it = m_transparentMesh.find(v.key);
        if (it != m_transparentMesh.end())
            m_backend.drawMesh(it->second);
    }
    m_backend.setBlend(false);
    m_backend.setDepthWrite(true);

    m_stats.drawCalls = m_backend.stats().drawCalls;
    m_stats.triangles = m_backend.stats().triangles;

    // particles (additive points)
    if (!m_particles.empty() && quality.particleBudget() > 0) {
        drawParticles();
    }

    m_stats.visibleChunks = (int)visible.size();
}

// particle drawing: simple GL_POINTS (cheap on GLES2)
void Renderer::drawParticles() {
    const int cap = 512;
    static float pbuf[cap * 4];
    static float pcol[cap * 4];
    int n = 0;
    for (const Particle& p : m_particles) {
        if (n >= cap) break;
        pbuf[n * 4 + 0] = p.pos.x;
        pbuf[n * 4 + 1] = p.pos.y;
        pbuf[n * 4 + 2] = p.pos.z;
        pbuf[n * 4 + 3] = p.size;
        pcol[n * 4 + 0] = p.r; pcol[n * 4 + 1] = p.g; pcol[n * 4 + 2] = p.b;
        pcol[n * 4 + 3] = p.life / p.maxLife;
        ++n;
    }
    if (n == 0) return;
    m_backend.setAdditive(true);
    m_backend.drawPoints(pbuf, pcol, n);
    m_backend.setAdditive(false);
    m_stats.particles = n;
}

void Renderer::spawnParticles(const vec3& pos, int count, const vec3& color, float size, float speed, float life) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = pos;
        float a = (i * 2.399963f) * 3.14159265f / 180.f; // golden angle spread
        float b = (i * 1.570796f) * 3.14159265f / 180.f;
        p.vel = {std::cos(a) * std::cos(b) * speed, std::sin(b) * speed + 1.5f, std::sin(a) * std::cos(b) * speed};
        p.life = p.maxLife = life;
        p.r = color.x; p.g = color.y; p.b = color.z;
        p.size = size;
        m_particles.push_back(p);
    }
    if (m_particles.size() > 2048) m_particles.erase(m_particles.begin(), m_particles.end() - 2048);
}

void Renderer::updateParticles(float dt) {
    for (size_t i = m_particles.size(); i-- > 0;) {
        Particle& p = m_particles[i];
        p.life -= dt;
        if (p.life <= 0.f) { m_particles.erase(m_particles.begin() + (long)i); continue; }
        p.vel.y -= 9.8f * dt;
        p.pos = p.pos + p.vel * dt;
    }
}

} // namespace rb

// ROXBOX - greedy mesher
// Merges adjacent coplanar voxel faces into maximal quads -> minimal vertex
// count, the single biggest win for ultra-low-end GPUs (GLES2).
// Vertex layout (9 floats): pos(3) uv(2) shade(1) tint(1) wave(1) emi(1)
#include "mesher.h"
#include "../world/blocks.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace rb {

namespace {

// per-direction greedy meshing configuration
struct DirCfg {
    vec3 n;      // outward face normal
    vec3 u;      // in-plane axis (cross(u,v) == n)
    vec3 v;
    vec3 dstep;  // direction the face looks toward (== n)
};

// table order: +X, -X, +Y, -Y, +Z, -Z
constexpr DirCfg dirs[6] = {
    { { 1, 0, 0}, {0, 1, 0},  {0, 0, 1},  { 1, 0, 0} }, // +X : u=+Y, v=+Z
    { {-1, 0, 0}, {0, 1, 0},  {0, 0,-1},  {-1, 0, 0} }, // -X : u=+Y, v=-Z
    { { 0, 1, 0}, {0, 0, 1},  {1, 0, 0},  { 0, 1, 0} }, // +Y : u=+Z, v=+X
    { { 0,-1, 0}, {0, 0,-1},  {1, 0, 0},  { 0,-1, 0} }, // -Y : u=-Z, v=+X
    { { 0, 0, 1}, {1, 0, 0},  {0, 1, 0},  { 0, 0, 1} }, // +Z : u=+X, v=+Y
    { { 0, 0,-1}, {-1, 0, 0}, {0, 1, 0},  { 0, 0,-1} }, // -Z : u=-X, v=+Y
};

// grid cell encoding (uint16):
//   bits 0-4   : block id
//   bits 5-9   : tint palette index
//   bits 10-13 : depth k (0..15)
//   bit  15    : present flag
inline uint16_t encCell(uint8_t block, int tint, int k) {
    return (uint16_t)(((block & 0x1F) | ((tint & 0x1F) << 5) | ((k & 0xF) << 10) | 0x8000));
}
inline bool cellPresent(uint16_t c) { return (c & 0x8000) != 0; }
inline uint16_t cellProps(uint16_t c) { return c & 0x3FF; }
inline int cellDepth(uint16_t c) { return (c >> 10) & 0xF; }

inline bool isOpaque(uint8_t b) {
    return b != BLK_AIR && blockDefs[b].solid && !blockDefs[b].transparent;
}

inline int toI(float v) { return (int)std::lround(v); }

// face light factor from face normal (fixed sun direction, baked into shade)
float faceLight(const vec3& n) {
    const vec3 sun = normalize(vec3{0.45f, 0.82f, 0.36f});
    float diff = std::max(0.f, dot(n, sun));
    return std::min(1.08f, 0.42f + diff * 0.82f);
}

inline void emitVert(MeshData& mesh, float x, float y, float z, float u0, float v0,
                     float shade, int tint, int wave, int emi) {
    size_t i = mesh.verts.size();
    mesh.verts.resize(i + FLOATS_PER_VERT);
    float* p = &mesh.verts[i];
    p[0] = x; p[1] = y; p[2] = z;
    p[3] = u0; p[4] = v0;
    p[5] = shade;
    p[6] = (float)tint;
    p[7] = (float)wave;
    p[8] = (float)emi;
}

} // namespace

void buildChunkMesh(VoxelWorld& world, int cx, int cy, int cz, ChunkMesh& out) {
    out.cx = cx; out.cy = cy; out.cz = cz;
    out.ready = false;
    out.opaque.clear();
    out.transparent.clear();

    for (int d = 0; d < 6; ++d) {
        const DirCfg& cfg = dirs[d];
        uint16_t grid[CHUNK_SIZE * CHUNK_SIZE];
        std::memset(grid, 0, sizeof(grid));

        // outer face plane corner of the chunk on the -n side
        vec3 outer{ (float)(cx * CHUNK_SIZE), (float)(cy * CHUNK_SIZE), (float)(cz * CHUNK_SIZE) };
        if (cfg.n.x > 0) outer.x += CHUNK_SIZE - 1;
        if (cfg.n.y > 0) outer.y += CHUNK_SIZE - 1;
        if (cfg.n.z > 0) outer.z += CHUNK_SIZE - 1;
        vec3 sstep = cfg.n * -1.0f; // scan from outside inward

        // 1) first visible face per cell along the scan direction
        for (int u = 0; u < CHUNK_SIZE; ++u) {
            for (int v = 0; v < CHUNK_SIZE; ++v) {
                vec3 p = outer + cfg.u * (float)u + cfg.v * (float)v;
                for (int k = 0; k < CHUNK_SIZE; ++k) {
                    vec3 pos = p + sstep * (float)k;
                    int x = toI(pos.x), y = toI(pos.y), z = toI(pos.z);
                    uint8_t b = world.getBlock(x, y, z);
                    if (b == BLK_AIR) continue;
                    int nx = x + (int)cfg.n.x, ny = y + (int)cfg.n.y, nz = z + (int)cfg.n.z;
                    uint8_t nb = world.getBlock(nx, ny, nz);
                    if (isOpaque(nb)) continue;
                    if (blockDefs[b].transparent && nb == b) continue; // interior transparent face
                    int tint = world.voxelTint(x, y, z);
                    grid[v * CHUNK_SIZE + u] = encCell(b, tint, k);
                    break;
                }
            }
        }

        // 2) greedy rectangle expansion
        for (int v = 0; v < CHUNK_SIZE; ++v) {
            int u = 0;
            while (u < CHUNK_SIZE) {
                uint16_t cell = grid[v * CHUNK_SIZE + u];
                if (!cellPresent(cell)) { ++u; continue; }
                uint16_t props = cellProps(cell);
                int k0 = cellDepth(cell);

                int w = 1;
                while (u + w < CHUNK_SIZE) {
                    uint16_t c = grid[v * CHUNK_SIZE + u + w];
                    if (!cellPresent(c) || cellProps(c) != props || cellDepth(c) != k0) break;
                    ++w;
                }
                int h = 1;
                while (v + h < CHUNK_SIZE) {
                    bool ok = true;
                    for (int i = 0; i < w && ok; ++i) {
                        uint16_t c = grid[(v + h) * CHUNK_SIZE + u + i];
                        if (!cellPresent(c) || cellProps(c) != props || cellDepth(c) != k0) ok = false;
                    }
                    if (!ok) break;
                    ++h;
                }

                for (int yy = 0; yy < h; ++yy)
                    for (int xx = 0; xx < w; ++xx)
                        grid[(v + yy) * CHUNK_SIZE + u + xx] = 0;

                // 3) emit quad
                uint8_t blockId = (uint8_t)(props & 0x1F);
                int tint = (props >> 5) & 0x1F;
                int wave = blockDefs[blockId].wave ? 1 : 0;
                int emi = blockDefs[blockId].emissive ? 1 : 0;
                int tile = (int)blockId * 6 + d;
                float tileU = (float)(tile % 16) / 256.f;
                float tileV = (float)(tile / 16) / 256.f;

                // face plane origin for cell (u,v) at depth k0
                vec3 fp = outer + cfg.u * (float)u + cfg.v * (float)v + sstep * (float)(k0 - 1);

                vec3 c0 = fp;
                vec3 c1 = fp + cfg.u * (float)w;
                vec3 c2 = c1 + cfg.v * (float)h;
                vec3 c3 = fp + cfg.v * (float)h;

                float light = faceLight(cfg.n);
                MeshData& mesh = blockDefs[blockId].transparent ? out.transparent : out.opaque;
                size_t quadBase = mesh.verts.size() / FLOATS_PER_VERT;

                struct CornerInfo { vec3 pos; int iu, iv; };
                CornerInfo corners[4] = { {c0,0,0}, {c1,1,0}, {c2,1,1}, {c3,0,1} };
                float uvs[4][2] = {
                    {tileU, tileV},
                    {tileU + w * 16.f / 256.f, tileV},
                    {tileU + w * 16.f / 256.f, tileV + h * 16.f / 256.f},
                    {tileU, tileV + h * 16.f / 256.f},
                };

                for (int ci = 0; ci < 4; ++ci) {
                    const CornerInfo& c = corners[ci];
                    vec3 dirU = c.iu ? cfg.u : cfg.u * -1.0f;
                    vec3 dirV = c.iv ? cfg.v : cfg.v * -1.0f;
                    vec3 f = c.pos - cfg.n
                             - (c.iu ? cfg.u : vec3{0, 0, 0})
                             - (c.iv ? cfg.v : vec3{0, 0, 0});
                    vec3 s1p = f + dirU, s2p = f + dirV, s3p = f + dirU + dirV;
                    bool s1 = isOpaque(world.getBlock(toI(s1p.x), toI(s1p.y), toI(s1p.z)));
                    bool s2 = isOpaque(world.getBlock(toI(s2p.x), toI(s2p.y), toI(s2p.z)));
                    bool s3 = isOpaque(world.getBlock(toI(s3p.x), toI(s3p.y), toI(s3p.z)));
                    float ao;
                    if (s1 && s2) ao = 0.55f;
                    else if ((s1 && s3) || (s2 && s3)) ao = 0.78f;
                    else ao = 1.0f;
                    emitVert(mesh, c.pos.x, c.pos.y, c.pos.z, uvs[ci][0], uvs[ci][1],
                             light * ao, tint, wave, emi);
                }

                uint32_t i0 = (uint32_t)quadBase, i1 = i0 + 1, i2 = i0 + 2, i3 = i0 + 3;
                mesh.indices.push_back(i0); mesh.indices.push_back(i1); mesh.indices.push_back(i2);
                mesh.indices.push_back(i0); mesh.indices.push_back(i2); mesh.indices.push_back(i3);

                u += w;
            }
        }
    }
    out.ready = true;
}

} // namespace rb

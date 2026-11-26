// ROXBOX - greedy mesher
// Merges adjacent coplanar voxel faces into maximal quads -> minimal vertex
// count, the single biggest win for ultra-low-end GPUs (GLES2).
// Vertex layout (9 floats): pos(3) uv(2) shade(1) tint(1) wave(1) emi(1)
#pragma once
#include "../world/voxelworld.h"
#include <vector>
#include <cstdint>

namespace rb {

inline constexpr int VERTS_PER_QUAD = 6;
inline constexpr int FLOATS_PER_VERT = 9;

struct MeshData {
    std::vector<float> verts;      // FLOATS_PER_VERT * vertexCount
    std::vector<uint32_t> indices; // 3 per triangle

    void clear() { verts.clear(); indices.clear(); }
    size_t vertexCount() const { return verts.size() / FLOATS_PER_VERT; }
    bool empty() const { return verts.empty(); }
};

struct ChunkMesh {
    int cx = 0, cy = 0, cz = 0;
    MeshData opaque;
    MeshData transparent;
    bool ready = false;
};

// build the greedy mesh for one chunk (opaque + transparent passes)
void buildChunkMesh(VoxelWorld& world, int cx, int cy, int cz, ChunkMesh& out);

// statistics for the quality manager
struct MeshingStats {
    uint64_t totalVerts = 0;
    uint64_t totalTris = 0;
    uint64_t meshedChunks = 0;
};

} // namespace rb

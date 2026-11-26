// ROXBOX - a 16^3 voxel chunk
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include "../core/math.h"

namespace rb {

inline constexpr int CHUNK_SIZE = 16;
inline constexpr int CHUNK_VOL = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

struct Chunk {
    int cx = 0, cy = 0, cz = 0;
    uint8_t blocks[CHUNK_VOL];
    uint8_t tint[CHUNK_VOL]; // 0 = block default color, else palette index
    bool dirty = true;      // needs remesh
    bool present = true;    // marked for unload

    Chunk() {
        std::memset(blocks, 0, sizeof(blocks));
        std::memset(tint, 0, sizeof(tint));
    }

    inline int idx(int x, int y, int z) const {
        return (y * CHUNK_SIZE + z) * CHUNK_SIZE + x;
    }
    inline int idx(const vec3& v) const {
        return idx((int)v.x, (int)v.y, (int)v.z);
    }

    bool inBounds(int x, int y, int z) const {
        return x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE;
    }
    uint8_t get(int x, int y, int z) const {
        return inBounds(x, y, z) ? blocks[idx(x, y, z)] : 0;
    }
    void set(int x, int y, int z, uint8_t b) {
        if (inBounds(x, y, z)) blocks[idx(x, y, z)] = b;
    }
    uint8_t getTint(int x, int y, int z) const {
        return inBounds(x, y, z) ? tint[idx(x, y, z)] : 0;
    }
    void setTint(int x, int y, int z, uint8_t t) {
        if (inBounds(x, y, z)) tint[idx(x, y, z)] = t;
    }

    // linear index helper
    inline int i(int x, int y, int z) { return (y * CHUNK_SIZE + z) * CHUNK_SIZE + x; }
};

} // namespace rb

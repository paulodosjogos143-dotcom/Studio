// ROXBOX - voxel world: chunks, parts (Roblox-like instances), serialization
#pragma once
#include "chunk.h"
#include "blocks.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

namespace rb {

// a Roblox-like Part: an axis-aligned box of voxels with material + color
struct Part {
    int id = 0;
    std::string name;
    vec3 position{0, 0, 0};   // min corner, in voxel units (world space)
    vec3 size{1, 1, 1};       // extent in voxels
    Material material = Material::Plastic;
    int colorPalette = 0;     // 0 = material default, else palette index
    float transparency = 0.f; // 0..1
    bool canCollide = true;
    bool anchored = false;
    std::string scriptSource; // attached Lua source (empty = none)
    void* luaBinding = nullptr; // scripting layer binding (not serialized)

    bool contains(int x, int y, int z) const {
        int px = (int)position.x, py = (int)position.y, pz = (int)position.z;
        int sx = (int)(size.x + 0.001f), sy = (int)(size.y + 0.001f), sz = (int)(size.z + 0.001f);
        return x >= px && x < px + sx && y >= py && y < py + sy && z >= pz && z < pz + sz;
    }
};

class VoxelWorld {
public:
    VoxelWorld();

    // --- chunk access ---
    Chunk* getChunk(int cx, int cy, int cz);
    Chunk* ensureChunk(int cx, int cy, int cz);
    void unloadChunk(int cx, int cy, int cz);

    // --- voxel access (world coords) ---
    uint8_t getBlock(int x, int y, int z) const;
    uint8_t getBlock(const vec3& v) const { return getBlock((int)v.x, (int)v.y, (int)v.z); }
    void setBlock(int x, int y, int z, uint8_t b);
    void setBlock(const vec3& v, uint8_t b) { setBlock((int)v.x, (int)v.y, (int)v.z, b); }

    uint8_t getTint(int x, int y, int z) const;
    void setTint(int x, int y, int z, uint8_t t);

    // tint of a voxel: explicit tint, else the color of the part covering it
    int voxelTint(int x, int y, int z) const;

    // --- parts ---
    Part& addPart(const std::string& name, const vec3& pos, const vec3& size, Material m, int paletteColor);
    bool removePart(int id);
    Part* findPart(int id);
    Part* findPartAt(int x, int y, int z);
    const std::vector<Part>& parts() const { return m_parts; }
    std::vector<Part>& parts() { return m_parts; }

    // fill the voxels of a part into the chunk grid (overwrites)
    void materializePart(const Part& p);
    void dematerializePart(const Part& p); // restore air (best effort: set to air)

    // --- meshing queue ---
    void markDirty(int x, int y, int z);
    bool popDirtyChunk(int& cx, int& cy, int& cz);
    void clearDirtyQueue();
    size_t dirtyCount() const { return m_dirty.size(); }

    // --- serialization (ROXMAP binary) ---
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);

    vec3 spawn{8.f, 20.f, 8.f};
    size_t chunkCount() const { return m_chunks.size(); }
    uint64_t blocksPerChunk() const { return CHUNK_VOL; }
    const std::unordered_map<int64_t, Chunk>& chunks() const { return m_chunks; }

    // wipe the world (chunks, parts, dirty queue) for "New World"
    void clear();

    // chunk key (public so the renderer can key its mesh cache identically)
    static int64_t chunkKey(int cx, int cy, int cz);

private:
    std::unordered_map<int64_t, Chunk> m_chunks;
    std::vector<Part> m_parts;
    std::vector<int64_t> m_dirty;
    std::mutex m_mutex;
    int m_nextPartId = 1;
};

// procedural default world (deterministic, seeded)
void generateDefaultWorld(VoxelWorld& world);

} // namespace rb

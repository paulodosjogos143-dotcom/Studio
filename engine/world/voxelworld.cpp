#include "voxelworld.h"
#include "../core/fs.h"
#include "../core/log.h"
#include <cstring>

namespace rb {

VoxelWorld::VoxelWorld() = default;

int64_t VoxelWorld::chunkKey(int cx, int cy, int cz) {
    // sign-safe packing
    return ((int64_t)(cx + 32768) << 40) | ((int64_t)(cy + 32768) << 20) | (int64_t)(cz + 32768);
}

void VoxelWorld::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_chunks.clear();
    m_parts.clear();
    m_dirty.clear();
    m_nextPartId = 1;
}

Chunk* VoxelWorld::getChunk(int cx, int cy, int cz) {
    auto it = m_chunks.find(chunkKey(cx, cy, cz));
    return it == m_chunks.end() ? nullptr : &it->second;
}

Chunk* VoxelWorld::ensureChunk(int cx, int cy, int cz) {
    int64_t key = chunkKey(cx, cy, cz);
    auto it = m_chunks.find(key);
    if (it != m_chunks.end()) return &it->second;
    Chunk c;
    c.cx = cx; c.cy = cy; c.cz = cz;
    auto res = m_chunks.emplace(key, std::move(c));
    return &res.first->second;
}

void VoxelWorld::unloadChunk(int cx, int cy, int cz) {
    m_chunks.erase(chunkKey(cx, cy, cz));
}

uint8_t VoxelWorld::getBlock(int x, int y, int z) const {
    int cx = x >> 4, cy = y >> 4, cz = z >> 4;
    auto it = m_chunks.find(chunkKey(cx, cy, cz));
    if (it == m_chunks.end()) return BLK_AIR;
    int lx = x & 15, ly = y & 15, lz = z & 15;
    return it->second.blocks[(ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx];
}

void VoxelWorld::setBlock(int x, int y, int z, uint8_t b) {
    if (b == BLK_AIR && getBlock(x, y, z) == BLK_AIR) return;
    if (b != BLK_AIR && blockIsSolid(b)) { /* keep tint unless set */ }
    int cx = x >> 4, cy = y >> 4, cz = z >> 4;
    Chunk* c = ensureChunk(cx, cy, cz);
    int lx = x & 15, ly = y & 15, lz = z & 15;
    c->blocks[(ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx] = b;
    markDirty(x, y, z);
}

uint8_t VoxelWorld::getTint(int x, int y, int z) const {
    int cx = x >> 4, cy = y >> 4, cz = z >> 4;
    auto it = m_chunks.find(chunkKey(cx, cy, cz));
    if (it == m_chunks.end()) return 0;
    int lx = x & 15, ly = y & 15, lz = z & 15;
    return it->second.tint[(ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx];
}

void VoxelWorld::setTint(int x, int y, int z, uint8_t t) {
    int cx = x >> 4, cy = y >> 4, cz = z >> 4;
    Chunk* c = ensureChunk(cx, cy, cz);
    int lx = x & 15, ly = y & 15, lz = z & 15;
    c->tint[(ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx] = t;
    markDirty(x, y, z);
}

int VoxelWorld::voxelTint(int x, int y, int z) const {
    uint8_t explicitT = getTint(x, y, z);
    if (explicitT != 0) return explicitT;
    for (const Part& p : m_parts) {
        if (p.contains(x, y, z) && p.colorPalette > 0) return p.colorPalette;
    }
    return 0;
}

Part& VoxelWorld::addPart(const std::string& name, const vec3& pos, const vec3& size, Material m, int paletteColor) {
    Part p;
    p.id = m_nextPartId++;
    p.name = name.empty() ? ("Part" + std::to_string(p.id)) : name;
    p.position = pos;
    p.size = size;
    p.material = m;
    p.colorPalette = paletteColor;
    materializePart(p);
    m_parts.push_back(p);
    return m_parts.back();
}

bool VoxelWorld::removePart(int id) {
    for (size_t i = 0; i < m_parts.size(); ++i) {
        if (m_parts[i].id == id) {
            dematerializePart(m_parts[i]);
            m_parts.erase(m_parts.begin() + (long)i);
            return true;
        }
    }
    return false;
}

Part* VoxelWorld::findPart(int id) {
    for (auto& p : m_parts) if (p.id == id) return &p;
    return nullptr;
}

Part* VoxelWorld::findPartAt(int x, int y, int z) {
    for (auto& p : m_parts) if (p.contains(x, y, z)) return &p;
    return nullptr;
}

void VoxelWorld::materializePart(const Part& p) {
    uint8_t block = materialToBlock(p.material);
    int px = (int)p.position.x, py = (int)p.position.y, pz = (int)p.position.z;
    int sx = (int)(p.size.x + 0.001f), sy = (int)(p.size.y + 0.001f), sz = (int)(p.size.z + 0.001f);
    for (int x = px; x < px + sx; ++x)
        for (int y = py; y < py + sy; ++y)
            for (int z = pz; z < pz + sz; ++z) {
                // skip water (never overwrite water with solids)
                if (getBlock(x, y, z) == BLK_WATER && block != BLK_WATER) continue;
                setBlock(x, y, z, block);
                setTint(x, y, z, (uint8_t)p.colorPalette);
            }
}

void VoxelWorld::dematerializePart(const Part& p) {
    int px = (int)p.position.x, py = (int)p.position.y, pz = (int)p.position.z;
    int sx = (int)(p.size.x + 0.001f), sy = (int)(p.size.y + 0.001f), sz = (int)(p.size.z + 0.001f);
    for (int x = px; x < px + sx; ++x)
        for (int y = py; y < py + sy; ++y)
            for (int z = pz; z < pz + sz; ++z) {
                if (getBlock(x, y, z) == BLK_WATER) continue;
                if (findPartAt(x, y, z) == &p) { // no other part overlaps this voxel
                    setBlock(x, y, z, BLK_AIR);
                    setTint(x, y, z, 0);
                }
            }
}

void VoxelWorld::markDirty(int x, int y, int z) {
    int cx = x >> 4, cy = y >> 4, cz = z >> 4;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dz = -1; dz <= 1; ++dz) {
                int64_t key = chunkKey(cx + dx, cy + dy, cz + dz);
                auto it = m_chunks.find(key);
                if (it != m_chunks.end()) {
                    it->second.dirty = true;
                    // insert into dirty queue if not present
                    bool found = false;
                    for (int64_t d : m_dirty) if (d == key) { found = true; break; }
                    if (!found) m_dirty.push_back(key);
                }
            }
}

bool VoxelWorld::popDirtyChunk(int& cx, int& cy, int& cz) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_dirty.empty()) return false;
    int64_t key = m_dirty.front();
    m_dirty.erase(m_dirty.begin());
    cx = (int)((key >> 40) - 32768);
    cy = (int)(((key >> 20) & 0xFFFFF) - 32768);
    cz = (int)((key & 0xFFFFF) - 32768);
    return true;
}

void VoxelWorld::clearDirtyQueue() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_dirty.clear();
}

// ---------------- serialization (ROXMAP) ----------------

bool VoxelWorld::saveToFile(const std::string& path) const {
    // header: magic(8) version(4) spawn(12) partCount(4) chunkCount(4)
    // parts: id(4) nameLen(4) name n chars pos(12) size(12) material(4) color(4)
    //        transparency(4) canCollide(1) anchored(1) scriptLen(4) script n chars
    // chunks: cx(4) cy(4) cz(4) blocks(4096) tint(4096)
    std::vector<uint8_t> out;
    out.reserve(64 + (size_t)m_parts.size() * 128 + m_chunks.size() * (4096 + 4096 + 12));

    auto put32 = [&](uint32_t v) { out.push_back((uint8_t)(v >> 24)); out.push_back((uint8_t)(v >> 16)); out.push_back((uint8_t)(v >> 8)); out.push_back((uint8_t)v); };
    auto putf = [&](float v) { uint32_t t; __builtin_memcpy(&t, &v, 4); put32(t); };
    auto putStr = [&](const std::string& s) { put32((uint32_t)s.size()); out.insert(out.end(), s.begin(), s.end()); };

    const char magic[8] = {'R','O','X','M','A','P','1','\0'};
    out.insert(out.end(), magic, magic + 8);
    put32(1);
    putf(spawn.x); putf(spawn.y); putf(spawn.z);
    put32((uint32_t)m_parts.size());
    put32((uint32_t)m_chunks.size());

    for (const Part& p : m_parts) {
        put32((uint32_t)p.id);
        putStr(p.name);
        putf(p.position.x); putf(p.position.y); putf(p.position.z);
        putf(p.size.x); putf(p.size.y); putf(p.size.z);
        put32((uint32_t)p.material);
        put32((uint32_t)p.colorPalette);
        putf(p.transparency);
        out.push_back(p.canCollide ? 1 : 0);
        out.push_back(p.anchored ? 1 : 0);
        putStr(p.scriptSource);
    }

    for (const auto& kv : m_chunks) {
        const Chunk& c = kv.second;
        put32((uint32_t)c.cx); put32((uint32_t)c.cy); put32((uint32_t)c.cz);
        out.insert(out.end(), c.blocks, c.blocks + CHUNK_VOL);
        out.insert(out.end(), c.tint, c.tint + CHUNK_VOL);
    }

    if (!fs::writeBytes(path, out)) {
        RB_LOG_E("failed to save map: %s", path.c_str());
        return false;
    }
    RB_LOG_I("map saved: %s (%zu parts, %zu chunks, %zu KB)", path.c_str(), m_parts.size(),
             m_chunks.size(), out.size() / 1024);
    return true;
}

bool VoxelWorld::loadFromFile(const std::string& path) {
    std::vector<uint8_t> data;
    if (!fs::readBytes(path, data)) {
        RB_LOG_E("failed to read map: %s", path.c_str());
        return false;
    }
    size_t pos = 0;
    auto take32 = [&]() -> uint32_t {
        if (pos + 4 > data.size()) return 0;
        uint32_t v = ((uint32_t)data[pos] << 24) | ((uint32_t)data[pos + 1] << 16) |
                     ((uint32_t)data[pos + 2] << 8) | (uint32_t)data[pos + 3];
        pos += 4;
        return v;
    };
    auto takeF = [&]() -> float { uint32_t t = take32(); float f; __builtin_memcpy(&f, &t, 4); return f; };
    auto takeStr = [&]() -> std::string {
        uint32_t n = take32();
        if (pos + n > data.size()) return {};
        std::string s((const char*)&data[pos], n);
        pos += n;
        return s;
    };

    const char magic[8] = {'R','O','X','M','A','P','1','\0'};
    if (data.size() < 8 || __builtin_memcmp(data.data(), magic, 8) != 0) {
        RB_LOG_E("bad map file (not a ROXMAP): %s", path.c_str());
        return false;
    }
    pos = 8;
    uint32_t version = take32();
    if (version != 1) { RB_LOG_E("unsupported map version %u", version); return false; }
    spawn.x = takeF(); spawn.y = takeF(); spawn.z = takeF();
    uint32_t partCount = take32();
    uint32_t chunkCount = take32();

    m_chunks.clear();
    m_parts.clear();
    m_dirty.clear();
    m_nextPartId = 1;

    for (uint32_t i = 0; i < partCount; ++i) {
        Part p;
        p.id = (int)take32();
        p.name = takeStr();
        p.position = {takeF(), takeF(), takeF()};
        p.size = {takeF(), takeF(), takeF()};
        p.material = (Material)take32();
        p.colorPalette = (int)take32();
        p.transparency = takeF();
        p.canCollide = data[pos] != 0; pos++;
        p.anchored = data[pos] != 0; pos++;
        p.scriptSource = takeStr();
        m_parts.push_back(p);
        if (p.id >= m_nextPartId) m_nextPartId = p.id + 1;
    }

    for (uint32_t i = 0; i < chunkCount; ++i) {
        int cx = (int)take32(), cy = (int)take32(), cz = (int)take32();
        Chunk* c = ensureChunk(cx, cy, cz);
        if (pos + CHUNK_VOL + CHUNK_VOL > data.size()) return false;
        __builtin_memcpy(c->blocks, &data[pos], CHUNK_VOL); pos += CHUNK_VOL;
        __builtin_memcpy(c->tint, &data[pos], CHUNK_VOL); pos += CHUNK_VOL;
        c->dirty = true;
        int64_t key = chunkKey(cx, cy, cz);
        bool found = false;
        for (int64_t d : m_dirty) if (d == key) { found = true; break; }
        if (!found) m_dirty.push_back(key);
    }

    // materialize part tints onto the loaded grid
    for (const Part& p : m_parts) {
        int px = (int)p.position.x, py = (int)p.position.y, pz = (int)p.position.z;
        int sx = (int)(p.size.x + 0.001f), sy = (int)(p.size.y + 0.001f), sz = (int)(p.size.z + 0.001f);
        for (int x = px; x < px + sx; ++x)
            for (int y = py; y < py + sy; ++y)
                for (int z = pz; z < pz + sz; ++z)
                    setTint(x, y, z, (uint8_t)p.colorPalette);
    }

    RB_LOG_I("map loaded: %s (%zu parts, %zu chunks)", path.c_str(), m_parts.size(), m_chunks.size());
    return true;
}

// ---------------- default world generation ----------------

void generateDefaultWorld(VoxelWorld& world) {
    // deterministic terrain: height map from fbm, grass tops, dirt below,
    // stone deep, water in low areas, bedrock floor.
    const int W = 64, D = 64;
    float seaLevel = 18.f;
    world.spawn = {32.f, 30.f, 32.f};

    for (int x = 0; x < W; ++x) {
        for (int z = 0; z < D; ++z) {
            float h = fbm3({(float)x * 0.08f, 0.f, (float)z * 0.08f}, 1337u, 4) * 14.f + 22.f;
            int baseY = (int)std::floor(h);
            for (int y = 0; y < baseY; ++y) {
                uint8_t b;
                if (y == 0) b = BLK_BEDROCK;
                else if (y < baseY - 4) b = BLK_STONE;
                else if (y < baseY - 1) b = BLK_DIRT;
                else b = BLK_GRASS;
                if (baseY - 1 <= (int)seaLevel) b = BLK_WATER; // water fills lows
                world.setBlock(x, y, z, b);
            }
            // water column down to floor
            if (baseY <= (int)seaLevel) {
                for (int y = baseY; y <= (int)seaLevel; ++y) world.setBlock(x, y, z, BLK_WATER);
            }
        }
    }

    // spawn pad: a small platform of planks near spawn
    world.addPart("SpawnPad", {30.f, 14.f, 30.f}, {5.f, 1.f, 5.f}, Material::Wood, 0);
    // a few decorative parts (Roblox-style building pieces)
    world.addPart("Base", {12.f, 26.f, 12.f}, {6.f, 4.f, 6.f}, Material::Brick, 0);
    world.addPart("Tower", {20.f, 40.f, 20.f}, {2.f, 12.f, 2.f}, Material::Stone, 0);
    world.addPart("GlassDome", {40.f, 30.f, 12.f}, {5.f, 3.f, 5.f}, Material::Glass, 0);
    world.addPart("GoldBlock", {14.f, 31.f, 14.f}, {1.f, 1.f, 1.f}, Material::Gold, 0);
    world.addPart("NeonSign", {44.f, 33.f, 40.f}, {3.f, 1.f, 1.f}, Material::Neon, 10); // palette color
    world.addPart("LampPole", {38.f, 32.f, 38.f}, {1.f, 4.f, 1.f}, Material::Metal, 0);

    // light the lamp on top
    world.setBlock(38, 36, 38, BLK_LAMP);

    // script attached to spawn pad (Roblox-style)
    Part* pad = nullptr;
    for (auto& p : world.parts()) if (p.name == "SpawnPad") pad = &p;
    if (pad) {
        pad->scriptSource =
            "-- Spawn pad script\n"
            "local pad = script.Parent\n"
            "print('SpawnPad ready at', pad.Position)\n"
            "while true do\n"
            "    task.wait(1)\n"
            "end\n";
    }
    Part* tower = nullptr;
    for (auto& p : world.parts()) if (p.name == "Tower") tower = &p;
    if (tower) {
        tower->scriptSource =
            "-- Rotating tower light\n"
            "local light = script.Parent\n"
            "local t = 0\n"
            "while true do\n"
            "    t = t + 0.1\n"
            "    light.Transparency = 0.3 + math.sin(t) * 0.3\n"
            "    task.wait(0.1)\n"
            "end\n";
    }
}

} // namespace rb

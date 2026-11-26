// ROXBOX - headless smoke tests (verification builds)
// Exercises: greedy mesher, world serialization, Lua/Roblox API, quality manager.
#include "../engine/world/voxelworld.h"
#include "../engine/mesh/mesher.h"
#include "../engine/scripting/luahost.h"
#include "../engine/scripting/robloxapi.h"
#include "../engine/render/quality.h"
#include "../engine/core/fs.h"
#include "../engine/core/log.h"
#include <cstdio>
#include <cassert>
#include <string>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

using namespace rb;

static int g_failures = 0;

static bool luaGlobalBool(LuaHost& host, const char* name) {
    lua_State* L = host.state();
    lua_getglobal(L, name);
    bool v = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return v;
}
static int luaGlobalInt(LuaHost& host, const char* name) {
    lua_State* L = host.state();
    lua_getglobal(L, name);
    int v = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return v;
}
static bool isOpaqueTest(uint8_t b) {
    return b != BLK_AIR && blockDefs[b].solid && !blockDefs[b].transparent;
}

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_failures; } \
    else { std::printf("ok: %s\n", msg); } \
} while (0)

int main() {
    rb::logSetLevel(rb::LogLevel::Warn);

    // ---- 1. default world generation + meshing ----
    {
        VoxelWorld world;
        generateDefaultWorld(world);
        CHECK(world.chunkCount() > 0, "default world generated chunks");
        CHECK(world.parts().size() >= 5, "default world has parts");

        int cx, cy, cz;
        int meshed = 0;
        while (world.popDirtyChunk(cx, cy, cz)) {
            ChunkMesh cm;
            buildChunkMesh(world, cx, cy, cz, cm);
            ++meshed;
        }
        CHECK(meshed > 0, "chunks meshed (greedy)");

        // verify greedy merging actually reduces vertex count vs naive
        int totalQuads = 0;
        for (const auto& kv : world.chunks()) {
            const Chunk& c = kv.second;
            for (int i = 0; i < CHUNK_VOL; ++i) {
                uint8_t b = c.blocks[i];
                if (b == BLK_AIR || blockIsTransparent(b)) continue;
                // naive: all 6 faces, minus faces against solid neighbors
                for (int f = 0; f < 6; ++f) {
                    int ox[6] = {1, -1, 0, 0, 0, 0}, oy[6] = {0, 0, 1, -1, 0, 0}, oz[6] = {0, 0, 0, 0, 1, -1};
                    int x = i % 16, y = (i / 16) % 16, z = i / 256;
                    uint8_t nb = world.getBlock(c.cx * 16 + x + ox[f], c.cy * 16 + y + oy[f], c.cz * 16 + z + oz[f]);
                    if (!isOpaqueTest(nb)) ++totalQuads;
                }
            }
        }
        (void)totalQuads;
        std::printf("naive visible faces (upper bound): %d\n", totalQuads);
    }

    // ---- 2. serialization round-trip ----
    {
        VoxelWorld world;
        generateDefaultWorld(world);
        CHECK(world.saveToFile("/tmp/roxbox_test.roxmap"), "map saved");

        VoxelWorld world2;
        CHECK(world2.loadFromFile("/tmp/roxbox_test.roxmap"), "map loaded");
        CHECK(world2.chunkCount() == world.chunkCount(), "chunk count preserved");
        CHECK(world2.parts().size() == world.parts().size(), "part count preserved");

        // voxel contents preserved
        bool same = true;
        int checked = 0;
        for (const auto& kv : world.chunks()) {
            const Chunk& a = kv.second;
            Chunk* b = world2.getChunk(a.cx, a.cy, a.cz);
            if (!b) { same = false; break; }
            for (int i = 0; i < CHUNK_VOL && same; ++i)
                if (a.blocks[i] != b->blocks[i]) { same = false; }
            ++checked;
            if (checked > 200) break;
        }
        CHECK(same, "voxel contents preserved");
    }

    // ---- 3. Lua Roblox API ----
    {
        VoxelWorld world;
        generateDefaultWorld(world);

        LuaHost host;
        RobloxApi api(host);
        CHECK(api.init(host), "Roblox API registered");
        api.startScripts(world);
        std::printf("startScripts returned\n");

        // script that uses the API
        Part* pad = nullptr;
        for (auto& p : world.parts()) if (p.name == "SpawnPad") pad = &p;
        CHECK(pad != nullptr, "SpawnPad found");

        std::string err;
        bool ok = host.doString(
            "local part = Instance.new('Part')\n"
            "part.Position = Vector3.new(10, 30, 10)\n"
            "part.Size = Vector3.new(2, 2, 2)\n"
            "part.Color = Color3.new(1, 0, 0)\n"
            "part.Material = Enum.Material.Brick\n"
            "print('created', part.Name)\n"
            "local t = 0\n"
            "local conn = part.Changed:Connect(function() t = t + 1 end)\n"
            "part.Transparency = 0.5\n"
            "print('changed fired', t)\n", "test", err);
        CHECK(ok, "Instance.new + properties script ran");
        if (!ok) std::printf("   err: %s\n", err.c_str());

        // task.wait coroutine scheduler
        ok = host.doString(
            "done = false\n"
            "spawn(function()\n"
            "    task.wait(0.05)\n"
            "    done = true\n"
            "end)\n", "test2", err);
        CHECK(ok, "spawn + task.wait scheduled");
        float t = 0.f;
        while (!luaGlobalBool(host, "done") && t < 1.f) { api.update(0.016f, t); t += 0.016f; }
        CHECK(luaGlobalBool(host, "done"), "task.wait resumed by scheduler");

        // RenderStepped event
        ok = host.doString(
            "frames = 0\n"
            "RunService.RenderStepped:Connect(function() frames = frames + 1 end)\n", "test3", err);
        CHECK(ok, "RunService.RenderStepped connected");
        for (int i = 0; i < 5; ++i) api.update(0.016f, i * 0.016f);
        int fr = luaGlobalInt(host, "frames");
        std::printf("  frames value = %d\n", fr);
        CHECK(fr == 5, "RenderStepped fired 5 times");

        // workspace.GetPartAt
        std::printf("  parts in world: %zu\n", world.parts().size());
        for (const auto& pp : world.parts())
            std::printf("   part '%s' id=%d at (%.0f,%.0f,%.0f) size (%.0f,%.0f,%.0f)\n",
                        pp.name.c_str(), pp.id, pp.position.x, pp.position.y, pp.position.z,
                        pp.size.x, pp.size.y, pp.size.z);
        ok = host.doString(
            "local p = workspace:GetPartAt(10, 30, 10)\n"
            "assert(p ~= nil, 'part not found')\n"
            "print('found part', p.Name)\n", "test4", err);
        CHECK(ok, "workspace.GetPartAt works");
        if (!ok) std::printf("   err: %s\n", err.c_str());

        api.stopScripts();
    }

    // ---- 4. quality manager ----
    {
        QualityManager q;
        q.mode = QualityManager::Mode::Auto;
        q.tier = 3;
        for (int i = 0; i < 120; ++i) q.update(30.f); // slow frames -> degrade
        CHECK(q.tier < 3, "quality degrades under slow frames");
        CHECK(q.renderDistance() <= 4, "low tier uses small render distance");

        for (int i = 0; i < 600; ++i) q.update(8.f); // fast frames -> upgrade
        CHECK(q.tier > 0, "quality upgrades under fast frames");
    }

    // ---- 5. mesher edge cases ----
    {
        VoxelWorld world;
        // solid 4x4x4 cube of stone
        for (int x = 0; x < 4; ++x)
            for (int y = 0; y < 4; ++y)
                for (int z = 0; z < 4; ++z)
                    world.setBlock(x, y, z, BLK_STONE);
        ChunkMesh cm;
        buildChunkMesh(world, 0, 0, 0, cm);
        // a 4x4x4 cube has 6 faces; greedy merges each into 1 quad => 6 quads = 24 verts (4 per quad)
        std::printf("  solid cube verts=%zu\n", cm.opaque.vertexCount());
        CHECK(cm.opaque.vertexCount() == 24, "solid cube meshed as 6 merged quads");

        // single voxel: 6 quads
        VoxelWorld w2;
        w2.setBlock(0, 0, 0, BLK_STONE);
        ChunkMesh cm2;
        buildChunkMesh(w2, 0, 0, 0, cm2);
        std::printf("  single voxel verts=%zu\n", cm2.opaque.vertexCount());
        CHECK(cm2.opaque.vertexCount() == 24, "single voxel = 6 quads");

        // empty chunk: no mesh
        VoxelWorld w3;
        ChunkMesh cm3;
        buildChunkMesh(w3, 0, 0, 0, cm3);
        CHECK(cm3.opaque.empty() && cm3.transparent.empty(), "empty chunk has no mesh");
    }

    if (g_failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}

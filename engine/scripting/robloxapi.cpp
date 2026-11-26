#include "robloxapi.h"
#include "../core/log.h"
#include <cstring>
#include <cmath>
#include <algorithm>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace rb {

namespace {

// single script API instance (the engine has exactly one)
RobloxApi* g_api = nullptr;

// resume a coroutine inside a protected call so errors are caught, not panicked.
// upvalues: [1] lua_State* co, [2] int* nresOut, [3] int* resOut
int resumeCoroutine(lua_State* L) {
    lua_State* co = (lua_State*)lua_touserdata(L, lua_upvalueindex(1));
    int* nres = (int*)lua_touserdata(L, lua_upvalueindex(2));
    int* res = (int*)lua_touserdata(L, lua_upvalueindex(3));
    *res = lua_resume(co, L, 0, nres);
    return 0;
}

const char* materialNames[] = {
    "Plastic", "NeonPlastic", "Wood", "Metal", "Neon", "Brick", "Glass",
    "Stone", "Sand", "Leaves", "Water", "Gold", "Diamond", "Redstone", "Lamp"
};

struct V3 { float x, y, z; };
struct C3 { float r, g, b; };

void pushPtrUd(lua_State* L, void* p, const char* mt) {
    lua_newuserdata(L, sizeof(void*));
    *(void**)lua_topointer(L, -1) = p;
    luaL_getmetatable(L, mt);
    if (lua_istable(L, -1)) lua_setmetatable(L, -2);
    else lua_pop(L, 1);
}

void* ptrUd(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) return *(void**)lua_topointer(L, idx);
    return nullptr;
}

vec3 readVec3(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) {
        V3* v = (V3*)lua_topointer(L, idx);
        return {v->x, v->y, v->z};
    }
    return {(float)lua_tonumber(L, idx), (float)lua_tonumber(L, idx + 1), (float)lua_tonumber(L, idx + 2)};
}

void pushVec3(lua_State* L, const vec3& v) {
    V3 ud{v.x, v.y, v.z};
    lua_newuserdata(L, sizeof(V3));
    *(V3*)lua_topointer(L, -1) = ud;
    luaL_getmetatable(L, "rb.vector3");
    if (lua_istable(L, -1)) lua_setmetatable(L, -2);
    else lua_pop(L, 1);
}

void pushColor3(lua_State* L, const C3& c) {
    C3 ud = c;
    lua_newuserdata(L, sizeof(C3));
    *(C3*)lua_topointer(L, -1) = ud;
    luaL_getmetatable(L, "rb.color3");
    if (lua_istable(L, -1)) lua_setmetatable(L, -2);
    else lua_pop(L, 1);
}

int parseMaterial(lua_State* L, int idx) {
    if (lua_isnumber(L, idx)) return (int)std::max(0, std::min(14, (int)lua_tointeger(L, idx)));
    if (lua_isstring(L, idx)) {
        std::string s = lua_tostring(L, idx);
        for (int i = 0; i < 15; ++i)
            if (s == materialNames[i]) return i;
    }
    return 0;
}

vec3 colorToVec3(const C3& c) { return {c.r, c.g, c.b}; }

} // namespace

// ---------------- events ----------------

void RbEvent::connect(lua_State* L) {
    // method call: index 1 is self (event), the callback is at index 2
    if (!lua_isfunction(L, 2)) {
        luaL_error(L, "RbEvent.Connect expects a function");
        return;
    }
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    refs.push_back(ref);
}

void RbEvent::fire(lua_State* L) {
    for (int ref : refs) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        // pcall without a custom handler: the function on top is called directly;
        // on error Lua leaves a string message on the stack.
        int r = lua_pcall(L, 0, 0, 0);
        if (r != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            RB_LOG_W("event callback error: %s", msg ? msg : "?");
            lua_pop(L, 1);
        }
    }
}

void RbEvent::clear(lua_State* L) {
    for (int ref : refs) luaL_unref(L, LUA_REGISTRYINDEX, ref);
    refs.clear();
}

// ---------------- API ----------------

bool RobloxApi::init(LuaHost& host) {
    m_host = host;
    g_api = this;
    lua_State* L = host.state();
    if (!L) return false;

    // metatables
    luaL_newmetatable(L, "rb.part");
    lua_pushcfunction(L, l_part_index); lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_part_newindex); lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);

    luaL_newmetatable(L, "rb.event");
    lua_pushcfunction(L, l_event_index); lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_newmetatable(L, "rb.vector3");
    lua_pushcfunction(L, l_vector3_index); lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_newmetatable(L, "rb.color3");
    lua_pushcfunction(L, l_color3_index); lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_newmetatable(L, "rb.script");
    lua_pushcfunction(L, l_script_index); lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_script_newindex); lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);

    luaL_newmetatable(L, "rb.workspace");
    lua_pushcfunction(L, l_workspace_index); lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // Instance
    lua_newtable(L);
    lua_pushcfunction(L, l_instance_new); lua_setfield(L, -2, "new");
    lua_setglobal(L, "Instance");

    // workspace (table with rb.workspace metatable for __index methods)
    lua_newtable(L);
    luaL_getmetatable(L, "rb.workspace");
    if (lua_istable(L, -1)) lua_setmetatable(L, -2);
    else lua_pop(L, 1);
    lua_setglobal(L, "workspace");

    // game
    lua_newtable(L); // game
    lua_getglobal(L, "workspace");
    lua_setfield(L, -2, "Workspace");
    lua_newtable(L); // Players
    lua_newtable(L); // LocalPlayer
    luaL_newmetatable(L, "rb.player");
    lua_pushcfunction(L, l_player_index); lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_player_newindex); lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, "LocalPlayer");
    lua_setfield(L, -2, "Players");
    lua_setglobal(L, "game");

    // Vector3 / Color3
    lua_newtable(L);
    lua_pushcfunction(L, l_vector3_new); lua_setfield(L, -2, "new");
    lua_setglobal(L, "Vector3");
    lua_newtable(L);
    lua_pushcfunction(L, l_color3_new); lua_setfield(L, -2, "new");
    lua_setglobal(L, "Color3");

    // Enum
    lua_newtable(L);              // Enum
    lua_newtable(L);              // Enum.Material
    for (int i = 0; i < 15; ++i) {
        lua_pushstring(L, materialNames[i]);
        lua_pushinteger(L, i + 1);
        lua_rawset(L, -3);        // Material[name] = i+1
    }
    lua_setfield(L, -2, "Material");
    lua_setglobal(L, "Enum");

    // RunService (events fire every frame / heartbeat)
    lua_newtable(L);
    pushPtrUd(L, &m_renderStepped, "rb.event");
    lua_setfield(L, -2, "RenderStepped");
    pushPtrUd(L, &m_heartbeat, "rb.event");
    lua_setfield(L, -2, "Heartbeat");
    lua_setglobal(L, "RunService");

    // task
    lua_newtable(L);
    lua_pushcfunction(L, l_task_wait); lua_setfield(L, -2, "wait");
    lua_pushcfunction(L, l_task_delay); lua_setfield(L, -2, "delay");
    lua_setglobal(L, "task");

    lua_pushcfunction(L, l_spawn); lua_setglobal(L, "spawn");
    lua_pushcfunction(L, l_print); lua_setglobal(L, "print");
    lua_pushcfunction(L, l_print); lua_setglobal(L, "warn");
    lua_pushcfunction(L, l_destroy); lua_setglobal(L, "Destroy");
    lua_pushcfunction(L, l_task_wait); lua_setglobal(L, "wait");

    // small helper chunk
    std::string err;
    const char* helper =
        "math.clamp = function(v, a, b) return math.min(b, math.max(a, v)) end\n"
        "function pair_count(t) local n = 0 for _ in pairs(t) do n = n + 1 end return n end\n";
    if (!m_host.doString(helper, "roxbox.helper", err))
        RB_LOG_W("helper chunk failed: %s", err.c_str());

    RB_LOG_I("Roblox API registered");
    return true;
}

Part* RobloxApi::partFromLua(lua_State* L, int idx) {
    LuaPart* lp = (LuaPart*)ptrUd(L, idx);
    return lp ? lp->part : nullptr;
}

LuaPart* RobloxApi::luaPartAt(lua_State* L, int idx) {
    return (LuaPart*)ptrUd(L, idx);
}

void RobloxApi::rebuildWorkspaceParts() {
    lua_State* L = st();
    lua_getglobal(L, "workspace");
    lua_newtable(L);
    int i = 1;
    for (const LuaPart& lp : m_parts) {
        if (!lp.part) continue;
        pushPtrUd(L, (void*)&lp, "rb.part");
        lua_rawseti(L, -2, i++);
    }
    lua_setfield(L, -2, "Parts");
    lua_pop(L, 1);
}

void RobloxApi::startScripts(VoxelWorld& world) {
    m_world = &world;
    stopScripts();
    for (Part& p : world.parts()) {
        // ensure binding for every part
        if (!p.luaBinding) {
            LuaPart lp;
            lp.part = &p;
            m_parts.push_back(lp);
            p.luaBinding = &m_parts.back();
        }
    }
    for (Part& p : world.parts()) {
        if (p.scriptSource.empty()) continue;
        LuaScript s;
        s.parent = &p;
        s.source = p.scriptSource;
        m_scripts.push_back(s);
        LuaScript& stored = m_scripts.back();
        LuaPart* lp = (LuaPart*)p.luaBinding;
        if (lp) lp->changed; // touch
        runScript(stored);
    }
    rebuildWorkspaceParts();
    RB_LOG_I("scripts started (%zu bound, %zu running)", m_parts.size(), m_scripts.size());
}

void RobloxApi::stopScripts() {
    lua_State* L = st();
    if (L) {
        m_renderStepped.clear(L);
        m_heartbeat.clear(L);
        for (LuaPart& lp : m_parts) {
            lp.changed.clear(L);
            for (auto& kv : lp.propSignals) kv.second.clear(L);
            lp.propSignals.clear();
        }
    }
    m_parts.clear();
    m_scripts.clear();
    if (m_world) {
        for (Part& p : m_world->parts()) p.luaBinding = nullptr;
    }
}

void RobloxApi::runScript(LuaScript& s) {
    lua_State* L = st();
    if (!L || !s.parent) return;
    std::string chunk = "script:" + s.parent->name;
    if (luaL_loadbuffer(L, s.source.c_str(), s.source.size(), chunk.c_str()) != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        m_host.pushConsoleLine(std::string("[error] ") + (msg ? msg : "?"));
        RB_LOG_W("script error in '%s': %s", s.parent->name.c_str(), msg ? msg : "?");
        lua_pop(L, 1);
        return;
    }
    lua_State* co = lua_newthread(L);
    // lua_newthread pushes the new thread onto L's stack, so the compiled
    // function is now at index -2. Push a copy and move it to the coroutine.
    lua_pushvalue(L, -2);
    lua_xmove(L, co, 1);
    // set script global on the coroutine
    pushPtrUd(co, (void*)&s, "rb.script");
    lua_setglobal(co, "script");
    RB_LOG_I("running script on '%s'", s.parent->name.c_str());

    // run the coroutine under a protected call so a script error is reported, not fatal
    int nres = 0;
    int r = LUA_ERRRUN;
    // push error handler first, then the closure to call
    lua_pushcfunction(L, [](lua_State* s) -> int {
        const char* m = lua_tostring(s, 1);
        lua_pushstring(s, m ? m : "(unknown error)");
        return 1;
    });
    lua_pushlightuserdata(L, co);
    lua_pushlightuserdata(L, &nres);
    lua_pushlightuserdata(L, &r);
    lua_pushcclosure(L, resumeCoroutine, 3);
    int pc = lua_pcall(L, 0, 0, -2);
    lua_pop(L, 1); // err handler
    if (r == LUA_ERRRUN || pc != LUA_OK) {
        // error object is on the coroutine stack
        const char* msg = lua_tostring(co, -1);
        if (!msg) msg = lua_tostring(L, -1);
        m_host.pushConsoleLine(std::string("[error] ") + (msg ? msg : "?"));
        RB_LOG_W("script error in '%s': %s", s.parent->name.c_str(), msg ? msg : "?");
        if (lua_gettop(co) > 0) lua_pop(co, 1);
        if (lua_gettop(L) > 0) lua_pop(L, 1);
        return;
    }
    // yield is expected for scripts that loop with task.wait
    s.thread = co;
    s.running = true;
}

void RobloxApi::firePartChanged(LuaPart& lp, const char* prop) {
    lp.changed.fire(st());
    auto it = lp.propSignals.find(prop);
    if (it != lp.propSignals.end()) it->second.fire(st());
}

void RobloxApi::notifyPartChanged(Part* part, const char* prop) {
    if (!part || !part->luaBinding) return;
    firePartChanged(*(LuaPart*)part->luaBinding, prop);
}

void RobloxApi::movePartCpp(Part& part, const vec3& newPos) {
    movePart(part, newPos);
    notifyPartChanged(&part, "Position");
}

void RobloxApi::resizePartCpp(Part& part, const vec3& newSize) {
    resizePart(part, newSize);
    notifyPartChanged(&part, "Size");
}

void RobloxApi::recolorPartCpp(Part& part, int paletteColor) {
    part.colorPalette = paletteColor;
    int px = (int)part.position.x, py = (int)part.position.y, pz = (int)part.position.z;
    int sx = (int)(part.size.x + 0.001f), sy = (int)(part.size.y + 0.001f), sz = (int)(part.size.z + 0.001f);
    for (int x = px; x < px + sx; ++x)
        for (int y = py; y < py + sy; ++y)
            for (int z = pz; z < pz + sz; ++z)
                m_world->setTint(x, y, z, (uint8_t)paletteColor);
    notifyPartChanged(&part, "Color");
}

void RobloxApi::rematerializePartCpp(Part& part, Material m) {
    part.material = m;
    uint8_t block = materialToBlock(m);
    int px = (int)part.position.x, py = (int)part.position.y, pz = (int)part.position.z;
    int sx = (int)(part.size.x + 0.001f), sy = (int)(part.size.y + 0.001f), sz = (int)(part.size.z + 0.001f);
    for (int x = px; x < px + sx; ++x)
        for (int y = py; y < py + sy; ++y)
            for (int z = pz; z < pz + sz; ++z)
                m_world->setBlock(x, y, z, block);
    notifyPartChanged(&part, "Material");
}

void RobloxApi::update(float dt, float timeSec) {
    m_timeSec = timeSec;
    lua_State* L = st();
    if (!L) return;
    m_renderStepped.fire(L);
    m_heartbeat.fire(L);
    m_host.update(dt, timeSec);
}

bool RobloxApi::consumePlayerTeleport(vec3& out) {
    if (m_playerTeleport) {
        out = m_playerPos;
        m_playerTeleport = false;
        return true;
    }
    return false;
}

// ---------------- part property helpers ----------------

void RobloxApi::movePart(Part& old, const vec3& newPos) {
    Part tmp = old;
    LuaPart* lp = (LuaPart*)old.luaBinding; // capture binding before the part is erased
    m_world->removePart(old.id);
    tmp.position = newPos;
    Part& fresh = m_world->addPart(tmp.name, tmp.position, tmp.size, tmp.material, tmp.colorPalette);
    fresh.transparency = tmp.transparency;
    fresh.canCollide = tmp.canCollide;
    fresh.anchored = tmp.anchored;
    fresh.scriptSource = tmp.scriptSource;
    if (lp) {
        lp->part = &fresh;
        fresh.luaBinding = lp;
    }
}

void RobloxApi::resizePart(Part& p, const vec3& newSize) {
    Part tmp = p;
    LuaPart* lp = (LuaPart*)p.luaBinding; // capture before erase
    m_world->removePart(p.id);
    tmp.size = newSize;
    Part& fresh = m_world->addPart(tmp.name, tmp.position, tmp.size, tmp.material, tmp.colorPalette);
    fresh.transparency = tmp.transparency;
    fresh.canCollide = tmp.canCollide;
    fresh.anchored = tmp.anchored;
    fresh.scriptSource = tmp.scriptSource;
    if (lp) {
        lp->part = &fresh;
        fresh.luaBinding = lp;
    }
}

// ---------------- lua C functions ----------------

int RobloxApi::l_instance_new(lua_State* L) {
    const char* cls = lua_tostring(L, 1);
    if (!cls || std::strcmp(cls, "Part") != 0) {
        lua_pushnil(L);
        return 1;
    }
    if (!g_api->m_world) { lua_pushnil(L); return 1; }
    vec3 pos = g_api->m_world->spawn + vec3{0, 2, 0};
    Part& p = g_api->m_world->addPart("Part", pos, vec3{1, 1, 1}, Material::Plastic, 0);
    LuaPart lp;
    lp.part = &p;
    g_api->m_parts.push_back(lp);
    p.luaBinding = &g_api->m_parts.back();
    g_api->rebuildWorkspaceParts();
    pushPtrUd(L, (void*)&g_api->m_parts.back(), "rb.part");
    return 1;
}

int RobloxApi::l_part_index(lua_State* L) {
    LuaPart* lp = (LuaPart*)ptrUd(L, 1);
    if (!lp || !lp->part) { lua_pushnil(L); return 1; }
    Part& p = *lp->part;
    const char* name = lua_tostring(L, 2);
    if (!name) { lua_pushnil(L); return 1; }

    if (std::strcmp(name, "Position") == 0) pushVec3(L, p.position);
    else if (std::strcmp(name, "Size") == 0) pushVec3(L, p.size);
    else if (std::strcmp(name, "Color") == 0) {
        C3 c{palette[p.colorPalette].r / 255.f, palette[p.colorPalette].g / 255.f, palette[p.colorPalette].b / 255.f};
        pushColor3(L, c);
    } else if (std::strcmp(name, "Material") == 0) {
        lua_pushstring(L, materialNames[(int)p.material]);
    } else if (std::strcmp(name, "Transparency") == 0) {
        lua_pushnumber(L, p.transparency);
    } else if (std::strcmp(name, "CanCollide") == 0) {
        lua_pushboolean(L, p.canCollide);
    } else if (std::strcmp(name, "Anchored") == 0) {
        lua_pushboolean(L, p.anchored);
    } else if (std::strcmp(name, "Name") == 0) {
        lua_pushstring(L, p.name.c_str());
    } else if (std::strcmp(name, "Id") == 0) {
        lua_pushinteger(L, p.id);
    } else if (std::strcmp(name, "Changed") == 0) {
        pushPtrUd(L, &lp->changed, "rb.event");
    } else if (std::strcmp(name, "Script") == 0) {
        // find or create the LuaScript binding
        LuaScript* s = nullptr;
        for (LuaScript& ls : g_api->m_scripts)
            if (ls.parent == &p) { s = &ls; break; }
        if (!s) {
            LuaScript ls;
            ls.parent = &p;
            ls.source = p.scriptSource;
            g_api->m_scripts.push_back(ls);
            s = &g_api->m_scripts.back();
        }
        pushPtrUd(L, (void*)s, "rb.script");
    } else if (std::strcmp(name, "GetPropertyChangedSignal") == 0) {
        lua_pushcfunction(L, l_part_get_signal);
    } else if (std::strcmp(name, "Clone") == 0) {
        lua_pushcfunction(L, l_part_clone);
    } else if (std::strcmp(name, "Destroy") == 0) {
        lua_pushcfunction(L, l_destroy);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int RobloxApi::l_part_newindex(lua_State* L) {
    LuaPart* lp = (LuaPart*)ptrUd(L, 1);
    if (!lp || !lp->part || !g_api->m_world) return 0;
    Part& p = *lp->part;
    const char* name = lua_tostring(L, 2);
    if (!name) return 0;

    if (std::strcmp(name, "Position") == 0) {
        vec3 v = readVec3(L, 3);
        g_api->movePart(p, v);
        g_api->firePartChanged(*lp, name);
    } else if (std::strcmp(name, "Size") == 0) {
        vec3 v = readVec3(L, 3);
        g_api->resizePart(p, v);
        g_api->firePartChanged(*lp, name);
    } else if (std::strcmp(name, "Color") == 0) {
        C3 c;
        if (lua_isuserdata(L, 3)) {
            c = *(C3*)lua_topointer(L, 3);
        } else {
            c = {(float)lua_tonumber(L, 3), (float)lua_tonumber(L, 4), (float)lua_tonumber(L, 5)};
        }
        int idx = paletteNearest((uint8_t)(c.r * 255), (uint8_t)(c.g * 255), (uint8_t)(c.b * 255));
        p.colorPalette = idx;
        int px = (int)p.position.x, py = (int)p.position.y, pz = (int)p.position.z;
        int sx = (int)(p.size.x + 0.001f), sy = (int)(p.size.y + 0.001f), sz = (int)(p.size.z + 0.001f);
        for (int x = px; x < px + sx; ++x)
            for (int y = py; y < py + sy; ++y)
                for (int z = pz; z < pz + sz; ++z)
                    g_api->m_world->setTint(x, y, z, (uint8_t)idx);
        g_api->firePartChanged(*lp, name);
    } else if (std::strcmp(name, "Material") == 0) {
        int m = parseMaterial(L, 3);
        p.material = (Material)m;
        uint8_t block = materialToBlock((Material)m);
        int px = (int)p.position.x, py = (int)p.position.y, pz = (int)p.position.z;
        int sx = (int)(p.size.x + 0.001f), sy = (int)(p.size.y + 0.001f), sz = (int)(p.size.z + 0.001f);
        for (int x = px; x < px + sx; ++x)
            for (int y = py; y < py + sy; ++y)
                for (int z = pz; z < pz + sz; ++z)
                    g_api->m_world->setBlock(x, y, z, block);
        g_api->firePartChanged(*lp, name);
    } else if (std::strcmp(name, "Transparency") == 0) {
        p.transparency = (float)lua_tonumber(L, 3);
        g_api->firePartChanged(*lp, name);
    } else if (std::strcmp(name, "CanCollide") == 0) {
        p.canCollide = lua_toboolean(L, 3) != 0;
        g_api->firePartChanged(*lp, name);
    } else if (std::strcmp(name, "Anchored") == 0) {
        p.anchored = lua_toboolean(L, 3) != 0;
        g_api->firePartChanged(*lp, name);
    } else if (std::strcmp(name, "Name") == 0) {
        p.name = lua_tostring(L, 3);
        g_api->firePartChanged(*lp, name);
    }
    return 0;
}

int RobloxApi::l_part_get_signal(lua_State* L) {
    LuaPart* lp = (LuaPart*)ptrUd(L, 1);
    if (!lp) { lua_pushnil(L); return 1; }
    const char* prop = lua_tostring(L, 2);
    if (!prop) { lua_pushnil(L); return 1; }
    RbEvent& ev = lp->propSignals[prop];
    pushPtrUd(L, &ev, "rb.event");
    return 1;
}

int RobloxApi::l_part_clone(lua_State* L) {
    LuaPart* lp = (LuaPart*)ptrUd(L, 1);
    if (!lp || !lp->part || !g_api->m_world) { lua_pushnil(L); return 1; }
    Part& p = *lp->part;
    vec3 pos = p.position + vec3{10, 0, 0};
    Part& fresh = g_api->m_world->addPart("Clone" + p.name, pos, p.size, p.material, p.colorPalette);
    fresh.scriptSource = p.scriptSource;
    LuaPart clp;
    clp.part = &fresh;
    g_api->m_parts.push_back(clp);
    fresh.luaBinding = &g_api->m_parts.back();
    g_api->rebuildWorkspaceParts();
    pushPtrUd(L, (void*)&g_api->m_parts.back(), "rb.part");
    return 1;
}

int RobloxApi::l_event_index(lua_State* L) {
    RbEvent* ev = (RbEvent*)ptrUd(L, 1);
    if (!ev) { lua_pushnil(L); return 1; }
    const char* name = lua_tostring(L, 2);
    if (std::strcmp(name, "Connect") == 0) {
        lua_pushcfunction(L, l_event_connect);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int RobloxApi::l_event_connect(lua_State* L) {
    RbEvent* ev = (RbEvent*)ptrUd(L, 1);
    if (ev) ev->connect(L);
    return 0;
}

int RobloxApi::l_vector3_new(lua_State* L) {
    V3 v{(float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3)};
    lua_newuserdata(L, sizeof(V3));
    *(V3*)lua_topointer(L, -1) = v;
    luaL_getmetatable(L, "rb.vector3");
    if (lua_istable(L, -1)) lua_setmetatable(L, -2);
    else lua_pop(L, 1);
    return 1;
}

int RobloxApi::l_vector3_index(lua_State* L) {
    if (!lua_isuserdata(L, 1)) { lua_pushnil(L); return 1; }
    V3* v = (V3*)lua_topointer(L, 1);
    const char* name = lua_tostring(L, 2);
    if (!name) { lua_pushnil(L); return 1; }

    if (std::strcmp(name, "x") == 0) { lua_pushnumber(L, v->x); }
    else if (std::strcmp(name, "y") == 0) { lua_pushnumber(L, v->y); }
    else if (std::strcmp(name, "z") == 0) { lua_pushnumber(L, v->z); }
    else if (std::strcmp(name, "Add") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            V3* a = (V3*)lua_topointer(L, 1);
            vec3 b = readVec3(L, 2);
            pushVec3(L, {a->x + b.x, a->y + b.y, a->z + b.z});
            return 1;
        });
    } else if (std::strcmp(name, "Sub") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            V3* a = (V3*)lua_topointer(L, 1);
            vec3 b = readVec3(L, 2);
            pushVec3(L, {a->x - b.x, a->y - b.y, a->z - b.z});
            return 1;
        });
    } else if (std::strcmp(name, "Mul") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            V3* a = (V3*)lua_topointer(L, 1);
            float s = (float)lua_tonumber(L, 2);
            pushVec3(L, {a->x * s, a->y * s, a->z * s});
            return 1;
        });
    } else if (std::strcmp(name, "Magnitude") == 0 || std::strcmp(name, "Length") == 0) {
        lua_pushnumber(L, std::sqrt(v->x * v->x + v->y * v->y + v->z * v->z));
    } else if (std::strcmp(name, "Normalize") == 0) {
        float l = std::sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
        if (l > 1e-6f) pushVec3(L, {v->x / l, v->y / l, v->z / l});
        else pushVec3(L, {0, 0, 0});
    } else if (std::strcmp(name, "Dot") == 0) {
        vec3 b = readVec3(L, 2);
        lua_pushnumber(L, v->x * b.x + v->y * b.y + v->z * b.z);
    } else if (std::strcmp(name, "Cross") == 0) {
        vec3 b = readVec3(L, 2);
        pushVec3(L, {v->y * b.z - v->z * b.y, v->z * b.x - v->x * b.z, v->x * b.y - v->y * b.x});
    } else if (std::strcmp(name, "Distance") == 0) {
        vec3 b = readVec3(L, 2);
        lua_pushnumber(L, std::sqrt(std::pow(v->x - b.x, 2) + std::pow(v->y - b.y, 2) + std::pow(v->z - b.z, 2)));
    } else if (std::strcmp(name, "Lerp") == 0) {
        vec3 b = readVec3(L, 2);
        float t = (float)lua_tonumber(L, 3);
        pushVec3(L, {v->x + (b.x - v->x) * t, v->y + (b.y - v->y) * t, v->z + (b.z - v->z) * t});
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int RobloxApi::l_color3_new(lua_State* L) {
    C3 c{(float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3)};
    pushColor3(L, c);
    return 1;
}

int RobloxApi::l_color3_index(lua_State* L) {
    if (!lua_isuserdata(L, 1)) { lua_pushnil(L); return 1; }
    C3* c = (C3*)lua_topointer(L, 1);
    const char* name = lua_tostring(L, 2);
    if (!name) { lua_pushnil(L); return 1; }
    if (std::strcmp(name, "r") == 0) lua_pushnumber(L, c->r);
    else if (std::strcmp(name, "g") == 0) lua_pushnumber(L, c->g);
    else if (std::strcmp(name, "b") == 0) lua_pushnumber(L, c->b);
    else lua_pushnil(L);
    return 1;
}

int RobloxApi::l_task_wait(lua_State* L) {
    float sec = (float)lua_tonumber(L, 1);
    if (sec < 0.f) sec = 0.f;
    g_api->m_host.addWait(L, g_api->m_timeSec + sec);
    lua_yield(L, 0);
    return 0;
}

int RobloxApi::l_task_delay(lua_State* L) {
    float sec = (float)lua_tonumber(L, 1);
    if (sec < 0.f) sec = 0.f;
    int fnRef = luaL_ref(L, LUA_REGISTRYINDEX); // keep the function alive
    lua_State* co = lua_newthread(L);
    lua_pushnumber(co, sec);
    lua_pushinteger(co, fnRef);
    lua_pushcclosure(co, [](lua_State* L) -> int {
        float sec = (float)lua_tonumber(L, lua_upvalueindex(1));
        int fnRef = (int)lua_tointeger(L, lua_upvalueindex(2));
        lua_pushnumber(L, sec);
        l_task_wait(L); // yields until due
        lua_pushcfunction(L, [](lua_State* s) -> int {
            const char* m = lua_tostring(s, 1);
            lua_pushstring(s, m ? m : "(unknown error)");
            return 1;
        });
        lua_rawgeti(L, LUA_REGISTRYINDEX, fnRef);
        if (lua_pcall(L, 0, 0, -2) != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            g_api->m_host.pushConsoleLine(std::string("[error] ") + (msg ? msg : "?"));
            lua_pop(L, 1);
        }
        lua_pop(L, 1); // handler
        luaL_unref(L, LUA_REGISTRYINDEX, fnRef);
        return 0;
    }, 2);
    int nres = 0;
    int r = lua_resume(co, L, 0, &nres);
    if (r != LUA_OK && r != LUA_YIELD) {
        const char* msg = lua_tostring(co, -1);
        g_api->m_host.pushConsoleLine(std::string("[error] ") + (msg ? msg : "?"));
        lua_pop(co, 1);
    }
    return 0;
}

int RobloxApi::l_spawn(lua_State* L) {
    if (!lua_isfunction(L, 1)) return 0;
    lua_State* co = lua_newthread(L);
    lua_pushvalue(L, 1);
    lua_xmove(L, co, 1);
    // keep the coroutine alive in the registry (referenced) while it is suspended
    lua_pushvalue(L, -1); // the new thread is on top of L's stack
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    g_api->m_spawnRefs.push_back(ref);
    // resume the coroutine to run until it yields (task.wait) or finishes
    // (the function is already on top of co's stack via lua_xmove, so narg=0)
    int nres = 0;
    int r = lua_resume(co, L, 0, &nres);
    if (r != LUA_OK && r != LUA_YIELD) {
        const char* msg = lua_tostring(co, -1);
        if (!msg) msg = lua_tostring(L, -1);
        g_api->m_host.pushConsoleLine(std::string("[error] ") + (msg ? msg : "?"));
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        g_api->m_spawnRefs.pop_back();
        if (lua_gettop(co) > 0) lua_pop(co, 1);
    }
    return 0;
}

int RobloxApi::l_print(lua_State* L) {
    std::string line;
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        if (i > 1) line += "\t";
        switch (lua_type(L, i)) {
            case LUA_TSTRING: {
                size_t len = 0;
                const char* s = lua_tolstring(L, i, &len);
                line.append(s, len);
                break;
            }
            case LUA_TNUMBER: {
                line += lua_tostring(L, i);
                break;
            }
            case LUA_TBOOLEAN:
                line += lua_toboolean(L, i) ? "true" : "false";
                break;
            case LUA_TUSERDATA: {
                // pretty-print rb.vector3 / rb.color3 userdata
                if (lua_getmetatable(L, i)) {
                    // metatable is on top of the stack; compare its name
                    lua_getfield(L, -1, "__name");
                    const char* mtname = lua_tostring(L, -1);
                    if (mtname && std::strcmp(mtname, "rb.vector3") == 0) {
                        V3* v = (V3*)lua_topointer(L, i);
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "(%.1f, %.1f, %.1f)", v->x, v->y, v->z);
                        line += buf;
                    } else if (mtname && std::strcmp(mtname, "rb.color3") == 0) {
                        C3* c = (C3*)lua_topointer(L, i);
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "(%.2f, %.2f, %.2f)", c->r, c->g, c->b);
                        line += buf;
                    } else {
                        line += "<" + std::string(luaL_typename(L, i)) + ">";
                    }
                    lua_pop(L, 2); // __name, metatable
                } else {
                    line += "<" + std::string(luaL_typename(L, i)) + ">";
                }
                break;
            }
            default:
                line += "<" + std::string(luaL_typename(L, i)) + ">";
                break;
        }
    }
    g_api->m_host.pushConsoleLine(line);
    RB_LOG_I("[script] %s", line.c_str());
    return 0;
}

int RobloxApi::l_destroy(lua_State* L) {
    LuaPart* lp = (LuaPart*)ptrUd(L, 1);
    if (!lp || !lp->part || !g_api->m_world) return 0;
    int id = lp->part->id;
    g_api->m_world->removePart(id);
    // remove binding
    for (size_t i = 0; i < g_api->m_parts.size(); ++i) {
        if (g_api->m_parts[i].part && g_api->m_parts[i].part->id == id) {
            lua_State* L2 = g_api->st();
            g_api->m_parts[i].changed.clear(L2);
            g_api->m_parts.erase(g_api->m_parts.begin() + (long)i);
            break;
        }
    }
    g_api->rebuildWorkspaceParts();
    return 0;
}

int RobloxApi::l_workspace_index(lua_State* L) {
    const char* name = lua_tostring(L, 2);
    if (!name) { lua_pushnil(L); return 1; }
    if (std::strcmp(name, "GetPartAt") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            if (!g_api->m_world) { lua_pushnil(L); return 1; }
            // method call: index 1 is self, args at 2,3,4
            int x = (int)lua_tonumber(L, 2);
            int y = (int)lua_tonumber(L, 3);
            int z = (int)lua_tonumber(L, 4);
            Part* p = g_api->m_world->findPartAt(x, y, z);
            if (!p) { lua_pushnil(L); return 1; }
            if (!p->luaBinding) {
                LuaPart lp;
                lp.part = p;
                g_api->m_parts.push_back(lp);
                p->luaBinding = &g_api->m_parts.back();
            }
            pushPtrUd(L, p->luaBinding, "rb.part");
            return 1;
        });
    } else if (std::strcmp(name, "RemovePart") == 0) {
        lua_pushcfunction(L, l_destroy);
    } else if (std::strcmp(name, "Parts") == 0) {
        g_api->rebuildWorkspaceParts();
        lua_getglobal(L, "workspace");
        lua_getfield(L, -1, "Parts");
        lua_remove(L, -2);
    } else if (std::strcmp(name, "Spawn") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            if (g_api->m_world) {
                g_api->m_world->spawn = readVec3(L, 1);
            }
            return 0;
        });
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int RobloxApi::l_script_index(lua_State* L) {
    LuaScript* s = (LuaScript*)ptrUd(L, 1);
    if (!s) { lua_pushnil(L); return 1; }
    const char* name = lua_tostring(L, 2);
    if (!name) { lua_pushnil(L); return 1; }
    if (std::strcmp(name, "Parent") == 0) {
        if (!s->parent || !s->parent->luaBinding) { lua_pushnil(L); return 1; }
        pushPtrUd(L, s->parent->luaBinding, "rb.part");
    } else if (std::strcmp(name, "Source") == 0) {
        lua_pushlstring(L, s->source.c_str(), s->source.size());
    } else if (std::strcmp(name, "ClassName") == 0) {
        lua_pushstring(L, "Script");
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int RobloxApi::l_script_newindex(lua_State* L) {
    LuaScript* s = (LuaScript*)ptrUd(L, 1);
    if (!s || !s->parent) return 0;
    const char* name = lua_tostring(L, 2);
    if (std::strcmp(name, "Source") == 0) {
        size_t len = 0;
        const char* src = luaL_checklstring(L, 3, &len);
        s->source.assign(src, len);
        s->parent->scriptSource = s->source;
        // restart the script
        if (s->running) {
            s->running = false;
            s->thread = nullptr;
        }
        g_api->runScript(*s);
    }
    return 0;
}

int RobloxApi::l_player_index(lua_State* L) {
    const char* name = lua_tostring(L, 2);
    if (!name) { lua_pushnil(L); return 1; }
    if (std::strcmp(name, "Position") == 0) {
        pushVec3(L, g_api->m_playerPos);
    } else if (std::strcmp(name, "Health") == 0) {
        lua_pushnumber(L, 100.0);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int RobloxApi::l_player_newindex(lua_State* L) {
    const char* name = lua_tostring(L, 2);
    if (std::strcmp(name, "Position") == 0) {
        g_api->m_playerPos = readVec3(L, 3);
        g_api->m_playerTeleport = true;
    }
    return 0;
}

} // namespace rb

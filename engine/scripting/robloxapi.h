// ROXBOX - Roblox-like Lua API
// workspace, Instance.new("Part"), part properties (Position/Size/Color/
// Material/Transparency/CanCollide/Anchored), .Changed events,
// RunService.RenderStepped/Heartbeat, task.wait, spawn, print, Vector3, Color3.
#pragma once
#include "luahost.h"
#include "../world/voxelworld.h"
#include <map>
#include <vector>
#include <deque>
#include <string>

struct lua_State;

namespace rb {

struct RbEvent {
    std::vector<int> refs;
    void connect(lua_State* L);
    void fire(lua_State* L);
    void clear(lua_State* L);
};

struct LuaScript {
    Part* parent = nullptr;
    std::string source;
    lua_State* thread = nullptr;
    bool running = false;
};

struct LuaPart {
    Part* part = nullptr;
    RbEvent changed;
    std::map<std::string, RbEvent> propSignals;
};

class RobloxApi {
public:
    explicit RobloxApi(LuaHost& host) : m_host(host) {}

    bool init(LuaHost& host);

    // compile + run all scripts attached to the world's parts
    void startScripts(VoxelWorld& world);
    void stopScripts();

    // per-frame script driver
    void update(float dt, float timeSec);

    // engine-side property change notification (Studio edits etc.)
    void notifyPartChanged(Part* part, const char* prop);

    // C++-side part mutation that keeps script bindings + events intact
    void movePartCpp(Part& part, const vec3& newPos);
    void resizePartCpp(Part& part, const vec3& newSize);
    void recolorPartCpp(Part& part, int paletteColor);
    void rematerializePartCpp(Part& part, Material m);

    // player mirror / teleport
    void setPlayerPosition(const vec3& p) { m_playerPos = p; m_playerTeleport = true; }
    bool consumePlayerTeleport(vec3& out);

    const std::deque<std::string>& console() const { return m_host.console(); }

private:
    LuaHost& m_host;
    VoxelWorld* m_world = nullptr;
    lua_State* st() { return m_host.state(); }

    void movePart(Part& old, const vec3& newPos);
    void resizePart(Part& p, const vec3& newSize);

    // deque keeps references stable across push_back (pointers stored in Lua userdata)
    std::deque<LuaPart> m_parts;
    std::deque<LuaScript> m_scripts;
    // registry refs keeping spawned coroutines alive while suspended
    std::vector<int> m_spawnRefs;
    RbEvent m_renderStepped;
    RbEvent m_heartbeat;

    vec3 m_playerPos{0, 0, 0};
    bool m_playerTeleport = false;
    float m_timeSec = 0.f;

    // lua C functions
    static int l_instance_new(lua_State* L);
    static int l_part_index(lua_State* L);
    static int l_part_newindex(lua_State* L);
    static int l_part_get_signal(lua_State* L);
    static int l_part_clone(lua_State* L);
    static int l_event_index(lua_State* L);
    static int l_event_connect(lua_State* L);
    static int l_vector3_new(lua_State* L);
    static int l_vector3_index(lua_State* L);
    static int l_color3_new(lua_State* L);
    static int l_color3_index(lua_State* L);
    static int l_task_wait(lua_State* L);
    static int l_task_delay(lua_State* L);
    static int l_spawn(lua_State* L);
    static int l_print(lua_State* L);
    static int l_destroy(lua_State* L);
    static int l_workspace_index(lua_State* L);
    static int l_script_index(lua_State* L);
    static int l_script_newindex(lua_State* L);
    static int l_player_index(lua_State* L);
    static int l_player_newindex(lua_State* L);

    Part* partFromLua(lua_State* L, int idx);
    LuaPart* luaPartAt(lua_State* L, int idx);
    void rebuildWorkspaceParts();
    void runScript(LuaScript& s);
    void firePartChanged(LuaPart& lp, const char* prop);
};

} // namespace rb

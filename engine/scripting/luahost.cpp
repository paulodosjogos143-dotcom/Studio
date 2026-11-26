#include "luahost.h"
#include "../core/log.h"
#include <cstring>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace rb {

namespace {
// error handler: ensures a pcall failure always has a string on the stack
int luaHostErrHandler(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    lua_pushstring(L, msg ? msg : "(unknown error)");
    return 1;
}

// resume a suspended coroutine inside a protected call (upvalues: co, &nres, &res)
int luaResumeProtected(lua_State* L) {
    lua_State* co = (lua_State*)lua_touserdata(L, lua_upvalueindex(1));
    int* nres = (int*)lua_touserdata(L, lua_upvalueindex(2));
    int* res = (int*)lua_touserdata(L, lua_upvalueindex(3));
    *res = lua_resume(co, L, 0, nres);
    return 0;
}
}

LuaHost::LuaHost() {
    m_state = luaL_newstate();
    if (!m_state) {
        RB_LOG_E("failed to create Lua state");
        return;
    }
    luaL_openlibs(m_state);
    // install a panic handler so uncaught errors produce a message, not an abort
    lua_atpanic(m_state, [](lua_State* L) -> int {
        const char* m = lua_tostring(L, 1);
        lua_pushstring(L, m ? m : "[panic] unprotected Lua error");
        return 1;
    });
    RB_LOG_I("Lua host created");
}

LuaHost::~LuaHost() {
    if (m_state) lua_close(m_state);
}

bool LuaHost::doString(const std::string& source, const std::string& chunkName, std::string& err) {
    if (!m_state) { err = "no lua state"; return false; }
    if (luaL_loadbuffer(m_state, source.c_str(), (size_t)source.size(), chunkName.c_str()) != LUA_OK) {
        err = lua_tostring(m_state, -1);
        lua_pop(m_state, 1);
        return false;
    }
    // pcall without a custom handler: runtime errors are strings and land on the stack
    if (lua_pcall(m_state, 0, 0, 0) != LUA_OK) {
        size_t len = 0;
        const char* s = luaL_tolstring(m_state, -1, &len);
        err.assign(s, len);
        lua_pop(m_state, 1);
        return false;
    }
    return true;
}

void LuaHost::update(float dt, float timeSec) {
    if (!m_state) return;
    // fire due task.wait continuations
    for (size_t i = m_waits.size(); i-- > 0;) {
        WaitEntry& w = m_waits[i];
        if (timeSec < w.resumeAt) continue;
        int nres = 0;
        int r = LUA_ERRRUN;
        // resume under a protected call so errors don't panic
        lua_pushlightuserdata(m_state, w.co);
        lua_pushlightuserdata(m_state, &nres);
        lua_pushlightuserdata(m_state, &r);
        lua_pushcclosure(m_state, luaResumeProtected, 3);
        int pc = lua_pcall(m_state, 0, 0, 0);
        if (pc != LUA_OK) {
            const char* msg = lua_tostring(m_state, -1);
            RB_LOG_W("script error: %s", msg ? msg : "?");
            m_console.push_back(std::string("[error] ") + (msg ? msg : "?"));
            lua_pop(m_state, 1);
        }
        m_waits.erase(m_waits.begin() + (long)i);
        if (r != LUA_OK && r != LUA_YIELD) {
            const char* msg = lua_tostring(w.co, -1);
            RB_LOG_W("script error: %s", msg ? msg : "?");
            m_console.push_back(std::string("[error] ") + (msg ? msg : "?"));
            if (lua_gettop(w.co) > 0) lua_pop(w.co, 1);
        }
    }
}

} // namespace rb

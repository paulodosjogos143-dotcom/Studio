// ROXBOX - Lua 5.4 host with a coroutine scheduler (task.wait / spawn)
#pragma once
#include <string>
#include <vector>
#include <deque>
#include <cstdint>

struct lua_State;

namespace rb {

class LuaHost {
public:
    LuaHost();
    ~LuaHost();

    lua_State* state() { return m_state; }

    // run a chunk of Lua source on the main state
    bool doString(const std::string& source, const std::string& chunkName, std::string& err);

    // scheduler: resume coroutines whose task.wait delay has elapsed
    void update(float dt, float timeSec);

    // console capture (script print output)
    const std::deque<std::string>& console() const { return m_console; }
    void pushConsoleLine(const std::string& line) { m_console.push_back(line); }
    void clearConsole() { m_console.clear(); }

    // internal: called by the task.wait C function
    struct WaitEntry { lua_State* co; float resumeAt; };
    void addWait(lua_State* co, float resumeAt) { m_waits.push_back({co, resumeAt}); }

private:
    lua_State* m_state = nullptr;
    std::vector<WaitEntry> m_waits;
    std::deque<std::string> m_console;
};

} // namespace rb

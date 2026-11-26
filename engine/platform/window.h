// ROXBOX - platform window/context abstraction
#pragma once
#include "../input/input.h"
#include <string>

namespace rb {

struct WindowSpec {
    int width = 1280;
    int height = 720;
    std::string title = "ROXBOX";
    bool fullscreen = false;
};

class PlatformWindow {
public:
    virtual ~PlatformWindow() = default;
    virtual bool create(const WindowSpec& spec) = 0;
    virtual void destroy() = 0;
    virtual void swapBuffers() = 0;
    virtual void pollEvents() = 0;
    // returns the accumulated input state for this frame (flags cleared by caller)
    virtual InputState takeInput() = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool shouldClose() const = 0;
};

// factory: picks the correct platform implementation
PlatformWindow* createPlatformWindow();

} // namespace rb

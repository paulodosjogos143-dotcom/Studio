// ROXBOX - headless platform window (verification builds / CI)
#include "window.h"
#include "../core/log.h"

namespace rb {

class NullWindow : public PlatformWindow {
public:
    bool create(const WindowSpec& spec) override {
        m_w = spec.width;
        m_h = spec.height;
        RB_LOG_I("null window created %dx%d (headless)", m_w, m_h);
        return true;
    }
    void destroy() override {}
    void swapBuffers() override {}
    void pollEvents() override {}
    InputState takeInput() override { return InputState{}; }
    int width() const override { return m_w; }
    int height() const override { return m_h; }
    bool shouldClose() const override { return m_close; }
    void requestClose() { m_close = true; }

private:
    int m_w = 800, m_h = 600;
    bool m_close = false;
};

PlatformWindow* createPlatformWindow() {
    return new NullWindow();
}

} // namespace rb

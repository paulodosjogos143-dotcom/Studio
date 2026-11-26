// ROXBOX - Android platform window (NativeActivity + EGL + OpenGL ES 2.0)
// Compiled only for Android targets.
#if defined(__ANDROID__)

#include "window.h"
#include "../core/log.h"
#include "../engine.h"
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/input.h>
#include <android/log.h>
#include <cmath>

#define LOG_TAG "ROXBOX"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rb {

namespace {

android_app* g_app = nullptr;
EGLDisplay g_display = EGL_NO_DISPLAY;
EGLConfig g_config = nullptr;
EGLSurface g_surface = EGL_NO_SURFACE;
EGLContext g_context = EGL_NO_CONTEXT;

int g_width = 0, g_height = 0;
InputState g_input;
bool g_close = false;

struct Pointer { int id = -1; float x = 0, y = 0, sx = 0, sy = 0; bool active = false; };
Pointer g_pointers[4];

Key androidKey(int code) {
    switch (code) {
        case AKEYCODE_W: return Key::W;
        case AKEYCODE_A: return Key::A;
        case AKEYCODE_S: return Key::S;
        case AKEYCODE_D: return Key::D;
        case AKEYCODE_SPACE: return Key::Space;
        case AKEYCODE_ENTER: return Key::Enter;
        case AKEYCODE_ESCAPE:
        case AKEYCODE_BACK: return Key::Escape;
        case AKEYCODE_DEL: return Key::Backspace;
        case AKEYCODE_TAB: return Key::Tab;
        case AKEYCODE_SHIFT: return Key::Shift;
        case AKEYCODE_CTRL: return Key::Control;
        case AKEYCODE_DPAD_UP: return Key::Up;
        case AKEYCODE_DPAD_DOWN: return Key::Down;
        case AKEYCODE_DPAD_LEFT: return Key::Left;
        case AKEYCODE_DPAD_RIGHT: return Key::Right;
        default: return Key::Unknown;
    }
}

void processInputEvent(AInputEvent* event) {
    if (!event) return;
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(event);
        int32_t code = AKeyEvent_getKeyCode(event);
        Key k = androidKey(code);
        if (k == Key::Unknown) return;
        if (action == AKEY_EVENT_ACTION_DOWN) {
            if (!g_input.keys[(int)k]) g_input.justPressed[(int)k] = true;
            g_input.keys[(int)k] = true;
        } else if (action == AKEY_EVENT_ACTION_UP) {
            g_input.keys[(int)k] = false;
        }
    } else if (type == AINPUT_EVENT_TYPE_TOUCH) {
        int32_t action = AMotionEvent_getAction(event);
        int32_t id = AMotionEvent_getPointerId(event, 0);
        if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            for (int i = 0; i < 4; ++i)
                if (!g_pointers[i].active) {
                    g_pointers[i].id = id;
                    g_pointers[i].x = AMotionEvent_getX(event, 0);
                    g_pointers[i].y = AMotionEvent_getY(event, 0);
                    g_pointers[i].sx = g_pointers[i].x;
                    g_pointers[i].sy = g_pointers[i].y;
                    g_pointers[i].active = true;
                    break;
                }
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            int32_t count = AMotionEvent_getPointerCount(event);
            for (int i = 0; i < 4 && i < count; ++i)
                if (g_pointers[i].active && g_pointers[i].id == AMotionEvent_getPointerId(event, i)) {
                    g_pointers[i].x = AMotionEvent_getX(event, i);
                    g_pointers[i].y = AMotionEvent_getY(event, i);
                }
        } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP
                   || action == AMOTION_EVENT_ACTION_CANCEL) {
            for (int i = 0; i < 4; ++i)
                if (g_pointers[i].active && g_pointers[i].id == id)
                    g_pointers[i].active = false;
        }
    }
}

// derive virtual joystick (left) and camera drag (right) from pointers
void updateVirtualControls() {
    g_input.joystick = {0, 0};
    g_input.touchActive = false;
    g_input.mouseX = -1.f;
    for (int i = 0; i < 4; ++i) {
        Pointer& p = g_pointers[i];
        if (!p.active) continue;
        g_input.touchActive = true;
        float dx = p.x - p.sx, dy = p.y - p.sy;
        if (p.sx < (float)g_width * 0.55f) {
            // virtual joystick
            float nx = std::max(-1.f, std::min(1.f, dx / ((float)g_width * 0.2f)));
            float ny = std::max(-1.f, std::min(1.f, -dy / ((float)g_height * 0.2f)));
            float mag = std::sqrt(nx * nx + ny * ny);
            if (mag > 1.f) { nx /= mag; ny /= mag; }
            g_input.joystick = {nx, ny};
        } else {
            // right side: camera drag + UI mouse
            g_input.mouseX = p.x;
            g_input.mouseY = p.y;
            g_input.wheel += dx * 0.01f;
        }
    }
}

Engine* g_engine = nullptr;
AndroidWindow* g_win = nullptr;
bool g_started = false;

void onAppCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_WINDOW_RESIZED:
            if (g_win && g_engine) {
                g_win->setNativeWindow((EGLNativeWindowType)app->window,
                                       app->requiredWindowWidth, app->requiredWindowHeight);
                g_engine->backend().resize(app->requiredWindowWidth, app->requiredWindowHeight);
                g_engine->renderer().resize(app->requiredWindowWidth, app->requiredWindowHeight);
            }
            break;
        case APP_CMD_DESTROY:
            if (g_engine) g_engine->requestQuit();
            break;
        default: break;
    }
}

} // namespace

class AndroidWindow : public PlatformWindow {
public:
    bool create(const WindowSpec& spec) override {
        // real GL setup happens in initEGL() once the surface exists
        g_width = spec.width;
        g_height = spec.height;
        return true;
    }

    bool initEGL() {
        g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (g_display == EGL_NO_DISPLAY) { LOGE("eglGetDisplay failed"); return false; }
        EGLint major = 0, minor = 0;
        if (!eglInitialize(g_display, &major, &minor)) { LOGE("eglInitialize failed"); return false; }
        LOGI("EGL %d.%d", major, minor);

        const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        EGLint numConfigs = 0;
        if (!eglChooseConfig(g_display, configAttribs, &g_config, 1, &numConfigs) || numConfigs < 1) {
            LOGE("eglChooseConfig failed");
            return false;
        }
        const EGLint surfaceAttribs[] = {EGL_NONE};
        g_surface = eglCreateWindowSurface(g_display, g_config, g_nativeWindow, surfaceAttribs);
        if (g_surface == EGL_NO_SURFACE) { LOGE("eglCreateWindowSurface failed"); return false; }
        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        g_context = eglCreateContext(g_display, g_config, EGL_NO_CONTEXT, contextAttribs);
        if (g_context == EGL_NO_CONTEXT) { LOGE("eglCreateContext failed"); return false; }
        if (!eglMakeCurrent(g_display, g_surface, g_surface, g_context)) {
            LOGE("eglMakeCurrent failed");
            return false;
        }
        glViewport(0, 0, g_width, g_height);
        return true;
    }

    void setNativeWindow(EGLNativeWindowType win, int w, int h) {
        g_nativeWindow = win;
        g_width = w;
        g_height = h;
    }

    void destroy() override {
        if (g_display != EGL_NO_DISPLAY) {
            eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
            if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
            eglTerminate(g_display);
            g_display = EGL_NO_DISPLAY;
        }
    }

    void swapBuffers() override {
        if (g_display != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE)
            eglSwapBuffers(g_display, g_surface);
    }

    void pollEvents() override {
        if (!g_app) return;
        AInputEvent* event = nullptr;
        // the glue drains the command queue (calling onAppCmd) and returns input events
        while (g_app->pollEvents(g_app, &event) == 1 && event) {
            processInputEvent(event);
            AInputEvent_unref(event);
            event = nullptr;
        }
        updateVirtualControls();
    }

    InputState takeInput() override { return g_input; }
    int width() const override { return g_width; }
    int height() const override { return g_height; }
    bool shouldClose() const override { return g_close; }

    void setApp(android_app* app) { g_app = app; }

private:
    EGLNativeWindowType g_nativeWindow = 0;
};

void android_main(android_app* app) {
    g_app = app;
    app->onAppCmd = onAppCmd;
    // wait for the surface, then run the engine loop
    while (!g_started) {
        AInputEvent* event = nullptr;
        app->pollEvents(app, &event);
        if (event) {
            if (g_win) processInputEvent(event);
            AInputEvent_unref(event);
        }
        if (app->window && !g_win) {
            // surface arrived; force command processing
            g_win = new AndroidWindow();
            g_win->setApp(app);
            g_win->setNativeWindow((EGLNativeWindowType)app->window,
                                   app->requiredWindowWidth, app->requiredWindowHeight);
            if (g_win->initEGL()) {
                EngineConfig cfg;
                cfg.mode = AppMode::Player;
                cfg.window.width = app->requiredWindowWidth;
                cfg.window.height = app->requiredWindowHeight;
                cfg.window.title = "ROXBOX";
                cfg.generateDefault = true;
                cfg.startTier = 1;
                cfg.externalWindow = g_win;
                g_engine = new Engine();
                if (g_engine->init(cfg)) {
                    g_started = true;
                } else {
                    LOGE("engine init failed");
                    return;
                }
            } else {
                delete g_win;
                g_win = nullptr;
            }
        }
    }
    g_engine->run();
    // cleanup
    if (g_engine) { delete g_engine; g_engine = nullptr; }
    if (g_win) { delete g_win; g_win = nullptr; }
}

PlatformWindow* createPlatformWindow() {
    return new AndroidWindow();
}

} // namespace rb

#endif

// ROXBOX - Linux desktop platform window (X11 + EGL + OpenGL ES 2.0)
// Compiled only for non-Android, non-Windows, non-headless Linux builds.
#if !defined(_WIN32) && !defined(__ANDROID__) && !defined(ROXBOX_HEADLESS)

#include "window.h"
#include "../core/log.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cstring>

namespace rb {

namespace {

Display* g_display = nullptr;
Window g_xwin = 0;
EGLDisplay g_egl = EGL_NO_DISPLAY;
EGLConfig g_config = nullptr;
EGLSurface g_surface = EGL_NO_SURFACE;
EGLContext g_context = EGL_NO_CONTEXT;
int g_width = 0, g_height = 0;
InputState g_input;
bool g_close = false;

Key xkeyToKey(int keysym) {
    switch (keysym) {
        case XK_w: case XK_W: return Key::W;
        case XK_a: case XK_A: return Key::A;
        case XK_s: case XK_S: return Key::S;
        case XK_d: case XK_D: return Key::D;
        case XK_q: case XK_Q: return Key::Q;
        case XK_e: case XK_E: return Key::E;
        case XK_r: case XK_R: return Key::R;
        case XK_f: case XK_F: return Key::F;
        case XK_g: case XK_G: return Key::G;
        case XK_space: return Key::Space;
        case XK_Return: return Key::Enter;
        case XK_Escape: return Key::Escape;
        case XK_BackSpace: return Key::Backspace;
        case XK_Tab: return Key::Tab;
        case XK_Shift_L: case XK_Shift_R: return Key::Shift;
        case XK_Control_L: case XK_Control_R: return Key::Control;
        case XK_Up: return Key::Up;
        case XK_Down: return Key::Down;
        case XK_Left: return Key::Left;
        case XK_Right: return Key::Right;
        case XK_0: case XK_9: break;
        default:
            if (keysym >= XK_a && keysym <= XK_z) return (Key)((int)Key::A + (keysym - XK_a));
            if (keysym >= XK_A && keysym <= XK_Z) return (Key)((int)Key::A + (keysym - XK_A));
            if (keysym >= XK_0 && keysym <= XK_9) return (Key)((int)Key::Digit0 + (keysym - XK_0));
            return Key::Unknown;
    }
    return Key::Unknown;
}

} // namespace

class LinuxWindow : public PlatformWindow {
public:
    bool create(const WindowSpec& spec) override {
        g_display = XOpenDisplay(nullptr);
        if (!g_display) { RB_LOG_E("XOpenDisplay failed"); return false; }
        int screen = XDefaultScreen(g_display);
        g_width = spec.width;
        g_height = spec.height;

        XSetWindowAttributes attrs{};
        attrs.event_mask = SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
                           KeyPressMask | KeyReleaseMask | PointerMotionMask | ExposureMask;
        g_xwin = XCreateWindow(g_display, RootWindow(g_display, screen),
                               100, 100, spec.width, spec.height, 0,
                               CopyFromParent, InputOutput, CopyFromParent, &attrs);
        XStoreName(g_display, g_xwin, spec.title.c_str());
        XMapWindow(g_display, g_xwin);

        g_egl = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (g_egl == EGL_NO_DISPLAY) { RB_LOG_E("eglGetDisplay failed"); return false; }
        EGLint major = 0, minor = 0;
        if (!eglInitialize(g_egl, &major, &minor)) { RB_LOG_E("eglInitialize failed"); return false; }

        const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        EGLint numConfigs = 0;
        if (!eglChooseConfig(g_egl, configAttribs, &g_config, 1, &numConfigs) || numConfigs < 1) {
            RB_LOG_E("eglChooseConfig failed");
            return false;
        }
        g_surface = eglCreateWindowSurface(g_egl, g_config, (EGLNativeWindowType)g_xwin, nullptr);
        if (g_surface == EGL_NO_SURFACE) { RB_LOG_E("eglCreateWindowSurface failed"); return false; }
        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        g_context = eglCreateContext(g_egl, g_config, EGL_NO_CONTEXT, contextAttribs);
        if (g_context == EGL_NO_CONTEXT) { RB_LOG_E("eglCreateContext failed"); return false; }
        if (!eglMakeCurrent(g_egl, g_surface, g_surface, g_context)) {
            RB_LOG_E("eglMakeCurrent failed");
            return false;
        }
        glViewport(0, 0, g_width, g_height);
        RB_LOG_I("Linux window created (%dx%d, EGL/GLES2)", g_width, g_height);
        return true;
    }

    void destroy() override {
        if (g_egl != EGL_NO_DISPLAY) {
            eglMakeCurrent(g_egl, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_egl, g_surface);
            if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_egl, g_context);
            eglTerminate(g_egl);
        }
        if (g_display && g_xwin) {
            XDestroyWindow(g_display, g_xwin);
            XCloseDisplay(g_display);
        }
        g_display = nullptr;
    }

    void swapBuffers() override {
        if (g_egl != EGL_NO_DISPLAY) eglSwapBuffers(g_egl, g_surface);
    }

    void pollEvents() override {
        if (!g_display) return;
        while (XPending(g_display)) {
            XEvent ev;
            XNextEvent(g_display, &ev);
            switch (ev.type) {
                case ClientMessage:
                    if ((int)ev.xclient.data.l[0] == 0x0) g_close = true;
                    break;
                case KeyPress: {
                    char buf[8];
                    Key k = xkeyToKey(XLookupKeyboard(&ev.xkey, buf, sizeof(buf), nullptr, nullptr));
                    if (k != Key::Unknown && !g_input.keys[(int)k]) g_input.justPressed[(int)k] = true;
                    g_input.keys[(int)k] = true;
                    if (buf[0]) g_input.typedChar = buf[0];
                    break;
                }
                case KeyRelease: {
                    char buf[8];
                    Key k = xkeyToKey(XLookupKeyboard(&ev.xkey, buf, sizeof(buf), nullptr, nullptr));
                    g_input.keys[(int)k] = false;
                    break;
                }
                case ButtonPress:
                    if (ev.xbutton.button == 1) { g_input.mouseDown[0] = true; g_input.mouseClicked[0] = true; }
                    if (ev.xbutton.button == 2) { g_input.mouseDown[1] = true; g_input.mouseClicked[1] = true; }
                    if (ev.xbutton.button == 4) g_input.wheel += 0.3f;
                    if (ev.xbutton.button == 5) g_input.wheel -= 0.3f;
                    break;
                case ButtonRelease:
                    if (ev.xbutton.button == 1) g_input.mouseDown[0] = false;
                    if (ev.xbutton.button == 2) g_input.mouseDown[1] = false;
                    break;
                case MotionNotify:
                    g_input.mouseX = (float)ev.xmotion.x;
                    g_input.mouseY = (float)ev.xmotion.y;
                    break;
                case ConfigureNotify:
                    g_width = ev.xconfigure.width;
                    g_height = ev.xconfigure.height;
                    glViewport(0, 0, g_width, g_height);
                    break;
                default: break;
            }
        }
    }

    InputState takeInput() override { return g_input; }
    int width() const override { return g_width; }
    int height() const override { return g_height; }
    bool shouldClose() const override { return g_close; }
};

PlatformWindow* createPlatformWindow() {
    return new LinuxWindow();
}

} // namespace rb

#endif

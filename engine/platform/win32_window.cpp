// ROXBOX - Windows platform window (Win32 + WGL, OpenGL 3.3 core)
// Compiled only for Windows targets.
#if defined(_WIN32) && !defined(ROXBOX_HEADLESS)

#include "window.h"
#include "../core/log.h"
#include <windows.h>
#include <wgl.h>

namespace rb {

namespace {

HWND g_hwnd = nullptr;
HINSTANCE g_instance = nullptr;
HDC g_hdc = nullptr;
HGLRC g_context = nullptr;
int g_width = 0, g_height = 0;
InputState g_input;
bool g_close = false;

Key vkToKey(UINT vk) {
    switch (vk) {
        case 'W': return Key::W; case 'A': return Key::A; case 'S': return Key::S;
        case 'D': return Key::D; case 'Q': return Key::Q; case 'E': return Key::E;
        case 'R': return Key::R; case 'F': return Key::F; case 'G': return Key::G;
        case VK_SPACE: return Key::Space;
        case VK_RETURN: return Key::Enter;
        case VK_ESCAPE: return Key::Escape;
        case VK_BACK: return Key::Backspace;
        case VK_TAB: return Key::Tab;
        case VK_SHIFT: return Key::Shift;
        case VK_CONTROL: return Key::Control;
        case VK_MENU: return Key::Alt;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_F1: return Key::F1;
        case VK_F2: return Key::F2;
        case VK_F3: return Key::F3;
        case VK_F4: return Key::F4;
        case VK_F5: return Key::F5;
        default:
            if (vk >= 'A' && vk <= 'Z') return (Key)((int)Key::A + (vk - 'A'));
            if (vk >= '0' && vk <= '9') return (Key)((int)Key::Digit0 + (vk - '0'));
            if (vk == VK_OEM_COMMA) return Key::comma;
            if (vk == VK_OEM_PERIOD) return Key::period;
            if (vk == VK_OEM_2) return Key::slash;
            if (vk == VK_OEM_1) return Key::semicolon;
            if (vk == VK_OEM_7) return Key::quote;
            if (vk == VK_OEM_4) return Key::bracketLeft;
            if (vk == VK_OEM_6) return Key::bracketRight;
            if (vk == VK_OEM_MINUS) return Key::minus;
            if (vk == VK_OEM_PLUS) return Key::equal;
            if (vk == VK_OEM_BACK) return Key::backslash;
            if (vk == VK_OEM_3) return Key::grave;
            return Key::Unknown;
    }
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            g_close = true;
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN: {
            Key k = vkToKey((UINT)wparam);
            if (k != Key::Unknown && !g_input.keys[(int)k]) g_input.justPressed[(int)k] = true;
            g_input.keys[(int)k] = true;
            return 0;
        }
        case WM_KEYUP: {
            Key k = vkToKey((UINT)wparam);
            g_input.keys[(int)k] = false;
            return 0;
        }
        case WM_CHAR: {
            if (wparam < 256) g_input.typedChar = (char)wparam;
            return 0;
        }
        case WM_MOUSEMOVE:
            g_input.mouseX = (float)GET_X_POSITION(lparam);
            g_input.mouseY = (float)GET_Y_POSITION(lparam);
            return 0;
        case WM_LBUTTONDOWN: g_input.mouseDown[0] = true; g_input.mouseClicked[0] = true; return 0;
        case WM_LBUTTONUP: g_input.mouseDown[0] = false; return 0;
        case WM_RBUTTONDOWN: g_input.mouseDown[1] = true; g_input.mouseClicked[1] = true; return 0;
        case WM_RBUTTONUP: g_input.mouseDown[1] = false; return 0;
        case WM_MOUSEWHEEL:
            g_input.wheel += (float)((SHORT)HIWORD(wparam) / 120.0);
            return 0;
        case WM_SIZE:
            g_width = LOWORD(lparam);
            g_height = HIWORD(lparam);
            if (g_context) {
                wglMakeCurrent(g_hdc, g_context);
                glViewport(0, 0, g_width, g_height);
            }
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

} // namespace

class Win32Window : public PlatformWindow {
public:
    bool create(const WindowSpec& spec) override {
        g_instance = GetModuleHandleA(nullptr);
        WNDCLASSA wc{};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = "ROXBOXWindow";
        RegisterClassA(&wc);

        g_width = spec.width;
        g_height = spec.height;
        g_close = false;

        g_hwnd = CreateWindowExA(0, wc.lpszClassName, spec.title.c_str(),
                                 WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                                 CW_USEDEFAULT, CW_USEDEFAULT, spec.width, spec.height,
                                 nullptr, nullptr, g_instance, nullptr);
        if (!g_hwnd) { RB_LOG_E("CreateWindowEx failed"); return false; }

        g_hdc = GetDC(g_hwnd);
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SWAP_LAYER_BUFFERS;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 0;
        pfd.iLayerType = PFD_DOUBLE_BUFFER;
        int fmt = ChoosePixelFormat(g_hdc, &pfd);
        if (!fmt || !SetPixelFormat(g_hdc, fmt, &pfd)) {
            RB_LOG_E("pixel format failed");
            return false;
        }
        g_context = wglCreateContext(g_hdc);
        if (!g_context || !wglMakeCurrent(g_hdc, g_context)) {
            RB_LOG_E("WGL context failed");
            return false;
        }
        // request OpenGL 3.3 core via WGL_ARB_create_context_profile
        typedef HGLRC(WINAPI* wglCreateContextAttribsARB_t)(HDC, int, int, const int*);
        auto createAttribs = (wglCreateContextAttribsARB_t)wglGetProcAddress("wglCreateContextAttribsARB");
        if (createAttribs) {
            int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                             WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
            HGLRC newCtx = createAttribs(g_hdc, 0, 16, attribs);
            if (newCtx) {
                wglDeleteContext(g_context);
                g_context = newCtx;
                wglMakeCurrent(g_hdc, g_context);
            }
        }
        ShowWindow(g_hwnd, SW_SHOW);
        glViewport(0, 0, g_width, g_height);
        RB_LOG_I("Win32 window created (%dx%d, GL 3.3 core)", g_width, g_height);
        return true;
    }

    void destroy() override {
        if (g_context) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(g_context); g_context = nullptr; }
        if (g_hdc && g_hwnd) ReleaseDC(g_hwnd, g_hdc);
        if (g_hwnd) DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }

    void swapBuffers() override {
        if (g_hdc) SwapBuffers(g_hdc);
    }

    void pollEvents() override {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    InputState takeInput() override {
        InputState in = g_input;
        return in;
    }

    int width() const override { return g_width; }
    int height() const override { return g_height; }
    bool shouldClose() const override { return g_close; }
};

PlatformWindow* createPlatformWindow() {
    return new Win32Window();
}

} // namespace rb

#endif

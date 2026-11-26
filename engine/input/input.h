// ROXBOX - unified input state (keyboard / mouse / touch / joystick)
// Platform windows translate OS events into this structure.
#pragma once
#include "../core/math.h"
#include <string>

namespace rb {

enum class Key {
    Unknown = 0, Space, Enter, Escape, Backspace, Tab,
    Shift, Control, Alt,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5,
    Digit6, Digit7, Digit8, Digit9,
    Up, Down, Left, Right,
    comma, period, slash, semicolon, quote, bracketLeft, bracketRight,
    minus, equal, backslash, grave,
    F1, F2, F3, F4, F5,
    KeyCount
};

struct InputState {
    bool keys[(int)Key::KeyCount] = {};
    bool justPressed[(int)Key::KeyCount] = {};

    float mouseX = 0.f, mouseY = 0.f; // pixels
    bool mouseDown[3] = {};
    bool mouseClicked[3] = {};
    float wheel = 0.f;

    // touch / virtual joystick (Android + mobile)
    bool touchActive = false;
    vec2 touchPos{0, 0};
    vec2 joystick{0, 0}; // normalized -1..1

    char typedChar = 0; // last typed character (0 = none this event)

    void clearFrameFlags() {
        for (int i = 0; i < (int)Key::KeyCount; ++i) justPressed[i] = false;
        for (int i = 0; i < 3; ++i) mouseClicked[i] = false;
        wheel = 0.f;
        typedChar = 0;
    }

    bool key(Key k) const { return keys[(int)k]; }
    bool pressed(Key k) const { return justPressed[(int)k]; }

    static Key charToKey(char c) {
        if (c >= 'a' && c <= 'z') return (Key)((int)Key::A + (c - 'a'));
        if (c >= 'A' && c <= 'Z') return (Key)((int)Key::A + (tolower(c) - 'a'));
        if (c >= '0' && c <= '9') return (Key)((int)Key::Digit0 + (c - '0'));
        switch (c) {
            case ' ': return Key::Space;
            case '\n': return Key::Enter;
            case '\t': return Key::Tab;
            case ',': return Key::comma;
            case '.': return Key::period;
            case '/': return Key::slash;
            case ';': return Key::semicolon;
            case '\'': return Key::quote;
            case '[': return Key::bracketLeft;
            case ']': return Key::bracketRight;
            case '-': return Key::minus;
            case '=': return Key::equal;
            case '\\': return Key::backslash;
            case '`': return Key::grave;
            default: return Key::Unknown;
        }
    }
};

} // namespace rb

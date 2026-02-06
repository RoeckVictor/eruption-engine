#pragma once

namespace engine::platform {

enum class KeyCode {
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Digits
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Arrow keys
    Left, Right, Up, Down,

    // Modifiers
    LeftShift, RightShift,
    LeftCtrl, RightCtrl,
    LeftAlt, RightAlt,

    // Special
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,
    Delete,

    // Punctuation
    LeftBracket,
    RightBracket,
    Comma,
    Period,
    Slash,
    Backslash,
    Semicolon,
    Apostrophe,
    GraveAccent,
    Minus,
    Equal,

    COUNT
};

enum class MouseButton {
    Left,
    Right,
    Middle,

    COUNT
};

} // namespace engine::platform

#include "engine/platform/Input.h"
#include "engine/platform/Window.h"
#include "engine/profiler/Profiler.h"
#include <cstring>

namespace engine::platform {

void Input::update(Window& window) {
    PROFILE_SCOPE("Input::update");
    memcpy(m_keys_prev, m_keys, sizeof(m_keys));
    memcpy(m_mouse_prev, m_mouse, sizeof(m_mouse));

    for (int i = 0; i < KEY_COUNT; i++) {
        m_keys[i] = window.is_key_down(static_cast<KeyCode>(i));
    }

    for (int i = 0; i < MOUSE_COUNT; i++) {
        m_mouse[i] = window.is_mouse_button_down(static_cast<MouseButton>(i));
    }

    window.get_cursor_position(m_mouse_x, m_mouse_y);

    // Consume accumulated scroll from Window
    m_scroll_y = window.consume_scroll();
}

bool Input::is_held(KeyCode key) const {
    int i = static_cast<int>(key);
    return i >= 0 && i < KEY_COUNT && m_keys[i];
}

bool Input::is_pressed(KeyCode key) const {
    int i = static_cast<int>(key);
    return i >= 0 && i < KEY_COUNT && m_keys[i] && !m_keys_prev[i];
}

bool Input::is_released(KeyCode key) const {
    int i = static_cast<int>(key);
    return i >= 0 && i < KEY_COUNT && !m_keys[i] && m_keys_prev[i];
}

bool Input::is_mouse_held(MouseButton button) const {
    int i = static_cast<int>(button);
    return i >= 0 && i < MOUSE_COUNT && m_mouse[i];
}

bool Input::is_mouse_pressed(MouseButton button) const {
    int i = static_cast<int>(button);
    return i >= 0 && i < MOUSE_COUNT && m_mouse[i] && !m_mouse_prev[i];
}

} // namespace engine::platform

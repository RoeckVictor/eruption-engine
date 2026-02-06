#include "engine/platform/Window.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace engine::platform {

// ---- Helper: recover the typed GLFW handle ----

static GLFWwindow* native(void* handle) {
    return static_cast<GLFWwindow*>(handle);
}

// ---- Key/mouse mapping (platform-specific, hidden from public API) ----

static int to_glfw_key(KeyCode key) {
    switch (key) {
        case KeyCode::A: return GLFW_KEY_A;
        case KeyCode::B: return GLFW_KEY_B;
        case KeyCode::C: return GLFW_KEY_C;
        case KeyCode::D: return GLFW_KEY_D;
        case KeyCode::E: return GLFW_KEY_E;
        case KeyCode::F: return GLFW_KEY_F;
        case KeyCode::G: return GLFW_KEY_G;
        case KeyCode::H: return GLFW_KEY_H;
        case KeyCode::I: return GLFW_KEY_I;
        case KeyCode::J: return GLFW_KEY_J;
        case KeyCode::K: return GLFW_KEY_K;
        case KeyCode::L: return GLFW_KEY_L;
        case KeyCode::M: return GLFW_KEY_M;
        case KeyCode::N: return GLFW_KEY_N;
        case KeyCode::O: return GLFW_KEY_O;
        case KeyCode::P: return GLFW_KEY_P;
        case KeyCode::Q: return GLFW_KEY_Q;
        case KeyCode::R: return GLFW_KEY_R;
        case KeyCode::S: return GLFW_KEY_S;
        case KeyCode::T: return GLFW_KEY_T;
        case KeyCode::U: return GLFW_KEY_U;
        case KeyCode::V: return GLFW_KEY_V;
        case KeyCode::W: return GLFW_KEY_W;
        case KeyCode::X: return GLFW_KEY_X;
        case KeyCode::Y: return GLFW_KEY_Y;
        case KeyCode::Z: return GLFW_KEY_Z;

        case KeyCode::Num0: return GLFW_KEY_0;
        case KeyCode::Num1: return GLFW_KEY_1;
        case KeyCode::Num2: return GLFW_KEY_2;
        case KeyCode::Num3: return GLFW_KEY_3;
        case KeyCode::Num4: return GLFW_KEY_4;
        case KeyCode::Num5: return GLFW_KEY_5;
        case KeyCode::Num6: return GLFW_KEY_6;
        case KeyCode::Num7: return GLFW_KEY_7;
        case KeyCode::Num8: return GLFW_KEY_8;
        case KeyCode::Num9: return GLFW_KEY_9;

        case KeyCode::F1:  return GLFW_KEY_F1;
        case KeyCode::F2:  return GLFW_KEY_F2;
        case KeyCode::F3:  return GLFW_KEY_F3;
        case KeyCode::F4:  return GLFW_KEY_F4;
        case KeyCode::F5:  return GLFW_KEY_F5;
        case KeyCode::F6:  return GLFW_KEY_F6;
        case KeyCode::F7:  return GLFW_KEY_F7;
        case KeyCode::F8:  return GLFW_KEY_F8;
        case KeyCode::F9:  return GLFW_KEY_F9;
        case KeyCode::F10: return GLFW_KEY_F10;
        case KeyCode::F11: return GLFW_KEY_F11;
        case KeyCode::F12: return GLFW_KEY_F12;

        case KeyCode::Left:  return GLFW_KEY_LEFT;
        case KeyCode::Right: return GLFW_KEY_RIGHT;
        case KeyCode::Up:    return GLFW_KEY_UP;
        case KeyCode::Down:  return GLFW_KEY_DOWN;

        case KeyCode::LeftShift:  return GLFW_KEY_LEFT_SHIFT;
        case KeyCode::RightShift: return GLFW_KEY_RIGHT_SHIFT;
        case KeyCode::LeftCtrl:   return GLFW_KEY_LEFT_CONTROL;
        case KeyCode::RightCtrl:  return GLFW_KEY_RIGHT_CONTROL;
        case KeyCode::LeftAlt:    return GLFW_KEY_LEFT_ALT;
        case KeyCode::RightAlt:   return GLFW_KEY_RIGHT_ALT;

        case KeyCode::Space:     return GLFW_KEY_SPACE;
        case KeyCode::Enter:     return GLFW_KEY_ENTER;
        case KeyCode::Escape:    return GLFW_KEY_ESCAPE;
        case KeyCode::Tab:       return GLFW_KEY_TAB;
        case KeyCode::Backspace: return GLFW_KEY_BACKSPACE;
        case KeyCode::Delete:    return GLFW_KEY_DELETE;

        case KeyCode::LeftBracket:  return GLFW_KEY_LEFT_BRACKET;
        case KeyCode::RightBracket: return GLFW_KEY_RIGHT_BRACKET;
        case KeyCode::Comma:        return GLFW_KEY_COMMA;
        case KeyCode::Period:       return GLFW_KEY_PERIOD;
        case KeyCode::Slash:        return GLFW_KEY_SLASH;
        case KeyCode::Backslash:    return GLFW_KEY_BACKSLASH;
        case KeyCode::Semicolon:    return GLFW_KEY_SEMICOLON;
        case KeyCode::Apostrophe:   return GLFW_KEY_APOSTROPHE;
        case KeyCode::GraveAccent:  return GLFW_KEY_GRAVE_ACCENT;
        case KeyCode::Minus:        return GLFW_KEY_MINUS;
        case KeyCode::Equal:        return GLFW_KEY_EQUAL;

        default: return GLFW_KEY_UNKNOWN;
    }
}

static int to_glfw_mouse(MouseButton button) {
    switch (button) {
        case MouseButton::Left:   return GLFW_MOUSE_BUTTON_LEFT;
        case MouseButton::Right:  return GLFW_MOUSE_BUTTON_RIGHT;
        case MouseButton::Middle: return GLFW_MOUSE_BUTTON_MIDDLE;
        default: return -1;
    }
}

// ---- File-local GLFW callbacks ----

static void glfw_error_callback(int error, const char* description) {
    ENGINE_ERR("GLFW Error %d: %s", error, description);
}

static void glfw_framebuffer_callback(GLFWwindow* window, int width, int height) {
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) win->on_framebuffer_resize(width, height);
}

static void glfw_scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) win->on_scroll((float)yoffset);
}

// ---- Internal event handlers ----

void Window::on_framebuffer_resize(int width, int height) {
    m_width = width;
    m_height = height;
}

void Window::on_scroll(float y_offset) {
    m_scroll_accum += y_offset;
}

// ---- Platform lifecycle (call once, not per-window) ----

bool Window::init_platform() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        ENGINE_ERR("Failed to init GLFW");
        return false;
    }
    return true;
}

void Window::shutdown_platform() {
    glfwTerminate();
}

// ---- Window lifecycle ----

bool Window::init(const char* title, int width, int height) {
    m_width = width;
    m_height = height;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    auto* handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!handle) {
        ENGINE_ERR("Failed to create GLFW window");
        return false;
    }
    m_handle = handle;

    glfwMakeContextCurrent(handle);
    glfwSetWindowUserPointer(handle, this);
    glfwSetFramebufferSizeCallback(handle, glfw_framebuffer_callback);
    glfwSetScrollCallback(handle, glfw_scroll_callback);
    set_vsync(true); // vsync on by default

    int version = gladLoadGL(glfwGetProcAddress);
    if (!version) {
        ENGINE_ERR("Failed to load OpenGL functions");
        glfwDestroyWindow(handle);
        m_handle = nullptr;
        return false;
    }

    ENGINE_LOG("OpenGL %d.%d loaded", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    return true;
}

void Window::shutdown() {
    if (m_handle) {
        glfwDestroyWindow(native(m_handle));
        m_handle = nullptr;
    }
}

bool Window::should_close() const {
    return glfwWindowShouldClose(native(m_handle));
}

void Window::set_should_close(bool close) {
    glfwSetWindowShouldClose(native(m_handle), close ? GLFW_TRUE : GLFW_FALSE);
}

void Window::swap_buffers() {
    glfwSwapBuffers(native(m_handle));
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::set_vsync(bool enabled) {
    glfwSwapInterval(enabled ? 1 : 0);
}

float Window::consume_scroll() {
    float val = m_scroll_accum;
    m_scroll_accum = 0.0f;
    return val;
}

// ---- Key/mouse polling (used by Input via friend access) ----

bool Window::is_key_down(KeyCode key) const {
    int glfw_key = to_glfw_key(key);
    return glfw_key != GLFW_KEY_UNKNOWN
        && glfwGetKey(native(m_handle), glfw_key) == GLFW_PRESS;
}

bool Window::is_mouse_button_down(MouseButton button) const {
    int glfw_btn = to_glfw_mouse(button);
    return glfw_btn >= 0
        && glfwGetMouseButton(native(m_handle), glfw_btn) == GLFW_PRESS;
}

void Window::get_cursor_position(double& x, double& y) const {
    glfwGetCursorPos(native(m_handle), &x, &y);
}

GLFWwindow* Window::glfw_handle() const {
    return native(m_handle);
}

} // namespace engine::platform

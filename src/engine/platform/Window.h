#pragma once

#include "engine/platform/KeyCode.h"

// Forward declaration for GLFW (must be in global namespace)
struct GLFWwindow;

namespace engine::platform {

class Input; // friend

/// Graphics API that the window should be configured for.
/// This determines what GLFW hints are set during window creation.
enum class GraphicsAPI {
    OpenGL,     // Request OpenGL 4.5 Core context
    Vulkan,     // No graphics context (Vulkan creates its own surface)
    None        // No graphics context
};

class Window {
public:
    Window() = default;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Platform-level init/shutdown (call once, before/after any Window instances)
    static bool init_platform();
    static void shutdown_platform();

    /// Initialize the window for a specific graphics API.
    /// @param api The graphics API to configure the window for
    bool init(const char* title, int width, int height, GraphicsAPI api = GraphicsAPI::OpenGL);
    void shutdown();
    bool should_close() const;
    void set_should_close(bool close);
    void swap_buffers();
    void poll_events();
    void set_vsync(bool enabled);

    int width() const { return m_width; }
    int height() const { return m_height; }

    /// Get the window position on screen (top-left corner).
    void get_position(int& x, int& y) const;

    /// Get the native GLFW window handle (for ImGui, RHI surface creation, etc.)
    GLFWwindow* glfw_handle() const;

    /// Get the OpenGL proc address loader function.
    /// Use this to initialize GLAD or similar GL loaders.
    /// Returns a function pointer with signature: void* (const char* name)
    static void* get_gl_proc_address(const char* name);

    // --- Internal event handlers (public for C callback access) ---
    // These are called from GLFW callbacks in Window.cpp. Do not call directly.
    void on_framebuffer_resize(int width, int height);
    void on_scroll(float y_offset);

private:
    friend class Input;

    // Scroll accumulation (consumed by Input each frame)
    float consume_scroll();

    // Raw polling used exclusively by Input
    bool is_key_down(KeyCode key) const;
    bool is_mouse_button_down(MouseButton button) const;
    void get_cursor_position(double& x, double& y) const;

    void* m_handle = nullptr; // opaque; GLFWwindow* in .cpp
    int m_width = 0;
    int m_height = 0;
    float m_scroll_accum = 0.0f;
};

} // namespace engine::platform

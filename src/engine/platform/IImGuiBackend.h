#pragma once

#include <memory>

namespace engine::platform {

class Window;

// Abstract interface for ImGui platform/renderer backends.
// This abstraction allows the editor to use ImGui without directly depending
// on GLFW, OpenGL, or any other specific backend implementation.
class IImGuiBackend {
public:
    virtual ~IImGuiBackend() = default;

    virtual bool init(Window& window) = 0;
    virtual void shutdown() = 0;

    virtual void new_frame() = 0;
    virtual void render_draw_data() = 0;

    virtual void update_platform_windows() = 0;

    virtual bool supports_viewports() const = 0;
};

std::unique_ptr<IImGuiBackend> create_imgui_backend();

}

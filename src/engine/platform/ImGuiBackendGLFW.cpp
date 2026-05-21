#include "engine/platform/IImGuiBackend.h"
#include "engine/platform/Window.h"
#include "engine/core/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace engine::platform {

/// GLFW + OpenGL3 ImGui backend implementation
class ImGuiBackendGLFW : public IImGuiBackend {
public:
    ~ImGuiBackendGLFW() override {
        if (m_initialized) {
            shutdown();
        }
    }

    bool init(Window& window) override {
        GLFWwindow* glfw_window = window.glfw_handle();
        if (!glfw_window) {
            ENGINE_ERR("ImGuiBackendGLFW: Window has no GLFW handle");
            return false;
        }

        const char* glsl_version = "#version 450";

        // Initialize GLFW backend (installs callbacks)
        if (!ImGui_ImplGlfw_InitForOpenGL(glfw_window, true)) {
            ENGINE_ERR("ImGuiBackendGLFW: Failed to initialize GLFW backend");
            return false;
        }

        // Initialize OpenGL3 backend
        if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
            ENGINE_ERR("ImGuiBackendGLFW: Failed to initialize OpenGL3 backend");
            ImGui_ImplGlfw_Shutdown();
            return false;
        }

        m_initialized = true;
        ENGINE_LOG("ImGuiBackendGLFW: Initialized with GLSL %s", glsl_version);
        return true;
    }

    void shutdown() override {
        if (m_initialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            m_initialized = false;
        }
    }

    void new_frame() override {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    void render_draw_data() override {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void update_platform_windows() override {
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_context);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    bool supports_viewports() const override {
        return true;
    }

private:
    bool m_initialized = false;
};

std::unique_ptr<IImGuiBackend> create_imgui_backend() {
    return std::make_unique<ImGuiBackendGLFW>();
}

} // namespace engine::platform

#include "PixArtApp.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <filesystem>
#include <string>

#include "editor/core/Constants.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"

// NOTE: This file uses OpenGL-specific ImGui backend (imgui_impl_opengl3).
// When adding support for other graphics APIs (Vulkan, D3D12, Metal),
// this should be refactored to use an RHI ImGui abstraction layer.
// For now, PixArt is OpenGL-only.

int main(int argc, char* argv[]) {
    // --- GLFW init ---
    if (!glfwInit()) {
        fprintf(stderr, "Failed to init GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        editor::constants::DEFAULT_WINDOW_WIDTH,
        editor::constants::DEFAULT_WINDOW_HEIGHT,
        "PixArt - Pixel Grid Editor", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // --- RHI init (handles GLAD loading internally) ---
    auto rhi_device = engine::rhi::create_rhi_device(
        engine::rhi::Backend::OpenGL,
        reinterpret_cast<engine::rhi::ProcAddressFunc>(glfwGetProcAddress));
    if (!rhi_device) {
        fprintf(stderr, "Failed to create RHI device\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    engine::rhi::set_current_device(rhi_device.get());

    // --- ImGui init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    // --- App init ---
    pixart::PixArtApp app;
    if (argc > 1) {
        app.init(argv[1]);
        // Update window title with filename
        std::string filename = std::filesystem::path(argv[1]).filename().string();
        std::string title = "PixArt - " + filename;
        glfwSetWindowTitle(window, title.c_str());
    } else {
        app.init();
    }

    // Set up window close callback to handle unsaved changes
    glfwSetWindowUserPointer(window, &app);
    glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
        auto* a = static_cast<pixart::PixArtApp*>(glfwGetWindowUserPointer(w));
        if (a->has_unsaved_changes()) {
            // Prevent immediate close, let app show dialog
            glfwSetWindowShouldClose(w, GLFW_FALSE);
            a->try_exit();
        }
    });

    // --- Main loop ---
    while (!glfwWindowShouldClose(window) && !app.should_close()) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.update();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        auto* ctx = engine::rhi::get_current_context();
        ctx->set_viewport(0, 0, display_w, display_h);
        ctx->clear(0.12f, 0.12f, 0.14f, 1.0f);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- Cleanup ---
    app.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    engine::rhi::set_current_device(nullptr);
    rhi_device.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

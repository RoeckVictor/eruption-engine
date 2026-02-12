#include "PixArtApp.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char* argv[]) {
    // --- GLFW init ---
    if (!glfwInit()) {
        fprintf(stderr, "Failed to init GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720,
        "PixArt - Pixel Grid Editor", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // --- GLAD init ---
    if (!gladLoadGL(glfwGetProcAddress)) {
        fprintf(stderr, "Failed to load OpenGL via GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

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
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- Cleanup ---
    app.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

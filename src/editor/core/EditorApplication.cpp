#include "EditorApplication.h"
#include "ProjectManager.h"
#include "EditorScene.h"
#include "EditorComponents.h"
#include "editor/panels/ProjectHubPanel.h"
#include "editor/panels/ConsolePanel.h"
#include "editor/panels/HierarchyPanel.h"
#include "editor/panels/InspectorPanel.h"
#include "editor/panels/ViewportPanel.h"
#include "editor/panels/FileBrowserPanel.h"
#include "editor/panels/SceneManagerPanel.h"
#include "editor/panels/AssetPreviewPanel.h"
#include "editor/panels/BuildSettingsPanel.h"
#include "editor/panels/ProjectSettingsPanel.h"
#include "editor/commands/EntityCommands.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/gizmos/Gizmo.h"

#include "engine/core/Engine.h"
#include "engine/core/Logger.h"
#include "engine/platform/Window.h"
#include "engine/platform/Input.h"

#include <imgui.h>
#include <filesystem>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace editor {

EditorApplication::EditorApplication()
    : m_project_manager(std::make_unique<ProjectManager>())
{
    // Set up the scene registry in context
    m_context.set_registry(&m_scene_registry);

    // Initialize runtime context with the editor's registry
    m_runtime.init(&m_scene_registry);

    // Link runtime context to editor context
    m_context.set_runtime(&m_runtime);
}

EditorApplication::~EditorApplication() = default;

bool EditorApplication::on_init(engine::Engine& engine) {
    // Push the editor scene (required by the engine)
    engine.scenes().push(std::make_unique<EditorScene>());

    init_imgui(engine);

    m_panel_manager.init();

    // Add all panels - pass EditorContext to panels that need it
    m_panel_manager.add_panel<ProjectHubPanel>(*m_project_manager, *this);
    m_panel_manager.add_panel<ConsolePanel>();
    m_panel_manager.add_panel<HierarchyPanel>(m_context);
    m_panel_manager.add_panel<InspectorPanel>(m_context);
    m_panel_manager.add_panel<ViewportPanel>(m_context);
    m_panel_manager.add_panel<FileBrowserPanel>();
    m_panel_manager.add_panel<SceneManagerPanel>();
    m_panel_manager.add_panel<AssetPreviewPanel>();
    m_panel_manager.add_panel<BuildSettingsPanel>();
    m_panel_manager.add_panel<ProjectSettingsPanel>(*m_project_manager);

    // If no project is loaded, show only the project hub
    if (!has_project()) {
        auto* hub = m_panel_manager.get_panel<ProjectHubPanel>();
        if (hub) {
            hub->set_visible(true);
            hub->set_closable(false);
        }

        // Hide other panels until project is loaded
        if (auto* p = m_panel_manager.get_panel<ConsolePanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<HierarchyPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<InspectorPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<ViewportPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<FileBrowserPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<SceneManagerPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<AssetPreviewPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<BuildSettingsPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<ProjectSettingsPanel>()) p->set_visible(false);
    }

    return true;
}

void EditorApplication::on_shutdown(engine::Engine& /*engine*/) {
    m_script_manager.shutdown();
    m_panel_manager.shutdown();
    shutdown_imgui();
}

void EditorApplication::on_update(engine::Engine& engine, float dt) {
    handle_shortcuts(engine);

    // Update script manager (handles async builds and file watching)
    m_script_manager.update();

    // Update build settings panel (handles async builds)
    if (auto* build_settings = m_panel_manager.get_panel<BuildSettingsPanel>()) {
        build_settings->update();
    }

    // Update runtime if playing
    if (m_runtime.is_playing()) {
        m_runtime.update(dt);
    }
}

void EditorApplication::on_render(engine::Engine& engine) {
    begin_frame();

    m_panel_manager.render_menu_bar();

    if (has_project()) {
        render_toolbar();
    } else {
        // No toolbar when no project is loaded
        m_panel_manager.set_toolbar_height(0.0f);
    }

    m_panel_manager.render();

    end_frame();
}

void EditorApplication::request_exit() {
    // TODO: Check for unsaved changes
    m_should_exit = true;
}

ProjectManager& EditorApplication::project_manager() {
    return *m_project_manager;
}

bool EditorApplication::has_project() const {
    return m_project_manager->has_project();
}

const std::string& EditorApplication::project_path() const {
    return m_project_manager->project_path();
}

void EditorApplication::init_imgui(engine::Engine& engine) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Set up ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;

    // Slightly darker background
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);

    // Initialize platform/renderer backends
    GLFWwindow* window = engine.window().glfw_handle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    m_imgui_initialized = true;
}

void EditorApplication::shutdown_imgui() {
    if (m_imgui_initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_imgui_initialized = false;
    }
}

void EditorApplication::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorApplication::end_frame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EditorApplication::render_toolbar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Position toolbar below menu bar
    float toolbar_height = 40.0f;

    // Set toolbar height for dockspace offset
    m_panel_manager.set_toolbar_height(toolbar_height);

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbar_height));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    if (ImGui::Begin("##Toolbar", nullptr, flags)) {
        // Play/Pause/Stop buttons with state-dependent appearance
        bool is_playing = m_runtime.is_playing();
        bool is_paused = m_runtime.is_paused();

        // Play button - changes to Resume when paused
        if (!is_playing) {
            // Not playing - show Play button
            if (ImGui::Button("Play")) {
                m_runtime.play();
            }
        } else if (is_paused) {
            // Paused - show Resume button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
            if (ImGui::Button("Resume")) {
                m_runtime.resume();
            }
            ImGui::PopStyleColor();
        } else {
            // Playing - show grayed Play button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
            ImGui::Button("Play");
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Pause button
        ImGui::BeginDisabled(!is_playing || is_paused);
        if (ImGui::Button("Pause")) {
            m_runtime.pause();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Stop button
        ImGui::BeginDisabled(!is_playing);
        if (ImGui::Button("Stop")) {
            m_runtime.stop();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Step button (only when paused)
        ImGui::BeginDisabled(!is_paused);
        if (ImGui::Button("Step")) {
            m_runtime.step_frame();
        }
        ImGui::EndDisabled();

        // Show play mode info
        if (is_playing) {
            ImGui::SameLine();
            ImGui::Text("|");
            ImGui::SameLine();

            if (is_paused) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "PAUSED");
            } else {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "PLAYING");
            }

            ImGui::SameLine();
            ImGui::TextDisabled("%.1fs | %llu frames", m_runtime.play_time(), m_runtime.frame_count());
        }

        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();

        // Gizmo mode buttons - Unity-style toolbar buttons
        auto* viewport = m_panel_manager.get_panel<ViewportPanel>();
        GizmoMode current_mode = viewport ? viewport->gizmo_renderer().mode() : GizmoMode::Translate;

        // Style for tool buttons
        ImVec2 button_size(50, 22);
        ImVec4 active_color(0.3f, 0.5f, 0.8f, 1.0f);
        ImVec4 active_hovered(0.4f, 0.6f, 0.9f, 1.0f);
        ImVec4 normal_color = ImGui::GetStyleColorVec4(ImGuiCol_Button);

        // Move button (W)
        bool is_translate = (current_mode == GizmoMode::Translate);
        if (is_translate) {
            ImGui::PushStyleColor(ImGuiCol_Button, active_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
        }
        if (ImGui::Button("Move", button_size)) {
            if (viewport) viewport->gizmo_renderer().set_mode(GizmoMode::Translate);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Translate Tool (W)");
        if (is_translate) ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 2);

        // Rotate button (E)
        bool is_rotate = (current_mode == GizmoMode::Rotate);
        if (is_rotate) {
            ImGui::PushStyleColor(ImGuiCol_Button, active_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
        }
        if (ImGui::Button("Rotate", button_size)) {
            if (viewport) viewport->gizmo_renderer().set_mode(GizmoMode::Rotate);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate Tool (E)");
        if (is_rotate) ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 2);

        // Scale button (R)
        bool is_scale = (current_mode == GizmoMode::Scale);
        if (is_scale) {
            ImGui::PushStyleColor(ImGuiCol_Button, active_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
        }
        if (ImGui::Button("Scale", button_size)) {
            if (viewport) viewport->gizmo_renderer().set_mode(GizmoMode::Scale);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale Tool (R)");
        if (is_scale) ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();

        // Coordinate space toggle (Local/World)
        bool is_local = m_context.is_local_space();
        if (ImGui::Button(is_local ? "Local" : "World", ImVec2(50, 22))) {
            m_context.set_local_space(!is_local);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle coordinate space for gizmos");

        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();

        // Grid/Snap toggles using context settings
        bool grid_visible = m_context.is_grid_visible();
        if (ImGui::Checkbox("Grid", &grid_visible)) {
            m_context.set_grid_visible(grid_visible);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle grid visibility (G)");

        ImGui::SameLine();

        bool snap_enabled = m_context.is_snap_enabled();
        if (ImGui::Checkbox("Snap", &snap_enabled)) {
            m_context.set_snap_enabled(snap_enabled);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle snap to grid");

        // Grid size input (only visible when snap is enabled)
        if (snap_enabled) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            float grid_size = m_context.grid_size();
            if (ImGui::DragFloat("##GridSize", &grid_size, 1.0f, 1.0f, 256.0f, "%.0f")) {
                m_context.set_grid_size(grid_size);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid size for snapping");
        }

        // Script status (right-aligned)
        ImGui::SameLine();
        float script_status_width = 200.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - script_status_width - 10.0f);

        if (m_script_manager.is_building()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Building scripts...");
        } else if (m_script_manager.are_scripts_loaded()) {
            ImGui::TextDisabled("Scripts: %zu types", m_script_manager.dll_manager().script_types().size());
        } else {
            ImGui::TextDisabled("No scripts");
        }
    }
    ImGui::End();

    ImGui::PopStyleVar();
}

void EditorApplication::handle_shortcuts(engine::Engine& engine) {
    using engine::platform::KeyCode;
    auto& input = engine.input();

    // Ctrl+Shift+N - New Project
    if (input.is_held(KeyCode::LeftCtrl) &&
        input.is_held(KeyCode::LeftShift) &&
        input.is_pressed(KeyCode::N)) {
        // TODO: New project dialog
    }

    // Ctrl+Shift+O - Open Project
    if (input.is_held(KeyCode::LeftCtrl) &&
        input.is_held(KeyCode::LeftShift) &&
        input.is_pressed(KeyCode::O)) {
        // TODO: Open project dialog
    }

    // Ctrl+S - Save Scene
    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::S)) {
        save_scene();
    }

    // Ctrl+Z - Undo
    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::Z)) {
        m_context.undo();
    }

    // Ctrl+Y - Redo (also Ctrl+Shift+Z)
    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::Y)) {
        m_context.redo();
    }
    if (input.is_held(KeyCode::LeftCtrl) && input.is_held(KeyCode::LeftShift) && input.is_pressed(KeyCode::Z)) {
        m_context.redo();
    }

    // Delete - Delete selected entities using command
    if (input.is_pressed(KeyCode::Delete)) {
        auto selection = m_context.selection(); // Copy because we'll modify it
        for (auto entity : selection) {
            if (m_scene_registry.valid(entity)) {
                auto cmd = std::make_unique<DeleteEntityCommand>(&m_scene_registry, &m_context, entity);
                m_context.execute_command(std::move(cmd));
            }
        }
    }

    // Ctrl+C - Copy selected entities
    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::C)) {
        m_context.copy_selection();
    }

    // Ctrl+V - Paste entities
    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::V)) {
        m_context.paste();
    }

    // Ctrl+D - Duplicate selected
    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::D)) {
        m_context.duplicate_selection();
    }

    // Ctrl+P - Play/Pause toggle
    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::P)) {
        if (!m_runtime.is_playing()) {
            m_runtime.play();
        } else if (m_runtime.is_paused()) {
            m_runtime.resume();
        } else {
            m_runtime.pause();
        }
    }

    // Escape - Stop play mode
    if (input.is_pressed(KeyCode::Escape)) {
        if (m_runtime.is_playing()) {
            m_runtime.stop();
        }
    }

    // F5 - Step frame (when paused)
    if (input.is_pressed(KeyCode::F5)) {
        if (m_runtime.is_paused()) {
            m_runtime.step_frame();
        }
    }

    // Ctrl+B - Build scripts
    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::B)) {
        rebuild_scripts();
    }

    // Gizmo mode shortcuts (only when not typing in a text field)
    if (!ImGui::GetIO().WantTextInput) {
        auto* viewport = m_panel_manager.get_panel<ViewportPanel>();
        if (viewport) {
            // W - Translate (Move) gizmo
            if (input.is_pressed(KeyCode::W)) {
                viewport->gizmo_renderer().set_mode(GizmoMode::Translate);
            }
            // E - Rotate gizmo
            if (input.is_pressed(KeyCode::E)) {
                viewport->gizmo_renderer().set_mode(GizmoMode::Rotate);
            }
            // R - Scale gizmo
            if (input.is_pressed(KeyCode::R)) {
                viewport->gizmo_renderer().set_mode(GizmoMode::Scale);
            }
        }

        // F - Focus on selected entity
        if (input.is_pressed(KeyCode::F)) {
            m_context.focus_on_selection();
        }

        // G - Toggle grid visibility
        if (input.is_pressed(KeyCode::G)) {
            m_context.set_grid_visible(!m_context.is_grid_visible());
        }
    }
}

void EditorApplication::on_project_loaded() {
    engine::Logger::instance().info("Editor", "Project loaded: %s", m_project_manager->project_info().name.c_str());

    // Initialize script manager for this project
    // Engine paths need to be absolute for CMake to find includes
    std::filesystem::path exe_dir = std::filesystem::current_path();
    // The _deps folder (with EnTT etc.) is in the build root, not build/Debug
    std::filesystem::path build_root = exe_dir.parent_path();  // Go up from Debug to build
    std::string engine_build_path = build_root.string();

    if (m_engine_src_path.empty()) {
        // Get the path relative to the executable and resolve to absolute
        // The executable is in build/Debug/, so ../../src gets us to the source folder
        std::filesystem::path src_path = exe_dir / "../../src";

        // Resolve to absolute canonical path
        std::error_code ec;
        std::filesystem::path canonical = std::filesystem::canonical(src_path, ec);
        if (!ec) {
            m_engine_src_path = canonical.string();
        } else {
            // Fallback: try relative to the TestGame directory structure
            m_engine_src_path = (exe_dir / "../../src").string();
        }
        engine::Logger::instance().info("Editor", "Engine source path: %s", m_engine_src_path.c_str());
        engine::Logger::instance().info("Editor", "Engine build path: %s", engine_build_path.c_str());
    }
    m_script_manager.init(m_project_manager->project_path(), m_engine_src_path, engine_build_path);

    // Show all editor panels
    if (auto* p = m_panel_manager.get_panel<ConsolePanel>()) p->set_visible(true);
    if (auto* p = m_panel_manager.get_panel<HierarchyPanel>()) p->set_visible(true);
    if (auto* p = m_panel_manager.get_panel<InspectorPanel>()) p->set_visible(true);
    if (auto* p = m_panel_manager.get_panel<ViewportPanel>()) p->set_visible(true);
    auto* asset_preview = m_panel_manager.get_panel<AssetPreviewPanel>();
    if (auto* file_browser = m_panel_manager.get_panel<FileBrowserPanel>()) {
        file_browser->set_visible(true);
        file_browser->set_root(m_project_manager->project_path());

        // Connect file browser selection to asset preview
        if (asset_preview) {
            file_browser->set_file_selected_callback([asset_preview](const std::string& path) {
                asset_preview->set_asset(path);
            });
        }
    }
    if (auto* p = m_panel_manager.get_panel<SceneManagerPanel>()) p->set_visible(true);
    if (asset_preview) asset_preview->set_visible(true);

    // Configure build settings panel
    if (auto* build_settings = m_panel_manager.get_panel<BuildSettingsPanel>()) {
        build_settings->set_visible(false);  // Hidden by default, accessible via menu
        build_settings->set_project_path(m_project_manager->project_path());
        build_settings->set_engine_paths(m_engine_src_path, engine_build_path);
    }

    // Hide project hub
    if (auto* hub = m_panel_manager.get_panel<ProjectHubPanel>()) {
        hub->set_visible(false);
    }

    // Create a new empty scene
    new_scene();
}

void EditorApplication::rebuild_scripts() {
    m_script_manager.rebuild();
}

void EditorApplication::new_scene() {
    // Clear the registry
    m_scene_registry.clear();
    m_context.clear_selection();
    m_context.clear_dirty();
    m_context.set_current_scene_path("");

    // Create a default camera entity
    auto camera = create_entity(m_scene_registry, "Main Camera");
    // TODO: Add camera component when we have one

    // Create a sample entity
    auto sample = create_entity(m_scene_registry, "Sample Entity");
    auto& transform = m_scene_registry.get<Transform>(sample);
    transform.x = 100.0f;
    transform.y = 100.0f;

    engine::Logger::instance().info("Editor", "New scene created");

    // Clear command history for new scene
    m_context.history().clear();
}

void EditorApplication::save_scene() {
    std::string path = m_context.current_scene_path();

    if (path.empty()) {
        // No path set, need to "Save As"
        // For now, use a default path in the project
        if (has_project()) {
            path = m_project_manager->project_path() + "/Assets/Scenes/Untitled.scene";
        } else {
            engine::Logger::instance().warning("Editor", "No project loaded, cannot save scene");
            return;
        }
    }

    SceneSerializer serializer(m_scene_registry);
    if (serializer.save(path)) {
        m_context.set_current_scene_path(path);
        m_context.clear_dirty();
        m_context.history().mark_saved();
        engine::Logger::instance().info("Editor", "Scene saved: %s", path.c_str());
    } else {
        engine::Logger::instance().error("Editor", "Failed to save scene: %s", serializer.last_error().c_str());
    }
}

bool EditorApplication::load_scene(const std::string& path) {
    SceneSerializer serializer(m_scene_registry);

    // Clear before loading
    m_scene_registry.clear();
    m_context.clear_selection();
    m_context.history().clear();

    if (serializer.load(path)) {
        m_context.set_current_scene_path(path);
        m_context.clear_dirty();
        m_context.history().mark_saved();
        engine::Logger::instance().info("Editor", "Scene loaded: %s", path.c_str());
        return true;
    } else {
        engine::Logger::instance().error("Editor", "Failed to load scene: %s", serializer.last_error().c_str());
        // Create a new empty scene on failure
        new_scene();
        return false;
    }
}

} // namespace editor

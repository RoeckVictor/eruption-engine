#include "EditorApplication.h"
#include "ProjectManager.h"
#include "EditorScene.h"
#include "EditorComponents.h"
#include "editor/panels/ProjectHubPanel.h"
#include "editor/panels/ConsolePanel.h"
#include "editor/panels/HierarchyPanel.h"
#include "editor/panels/InspectorPanel.h"
#include "editor/panels/ViewportPanel.h"
#include "editor/panels/GamePanel.h"
#include "editor/panels/FileBrowserPanel.h"
#include "editor/panels/SceneManagerPanel.h"
#include "editor/panels/AssetPreviewPanel.h"
#include "editor/panels/BuildSettingsPanel.h"
#include "editor/panels/ProjectSettingsPanel.h"
#include "editor/panels/PrefabEditorPanel.h"
#include "editor/commands/EntityCommands.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/gizmos/Gizmo.h"
#include "editor/icons/IconsFontAwesome6.h"

#include "engine/core/Engine.h"
#include "engine/core/Logger.h"
#include "engine/reflection/ReflectionInit.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/platform/Window.h"
#include "engine/platform/Input.h"

#include "engine/platform/PlatformUtils.h"

#include <imgui.h>
#include <imgui_internal.h>
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
    // Initialize engine component reflections
    engine::reflection::init_engine_reflections();

    // Initialize component type registry for dynamic component access
    init_component_type_registry();

    // Load default material library
    auto& mat_registry = engine::simulation::MaterialLibraryRegistry::instance();
    if (!mat_registry.load_library("default", "assets/materials/default.json")) {
        engine::Logger::instance().error("EditorApp", "Failed to load default material library");
    }

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
    m_panel_manager.add_panel<GamePanel>(m_context);
    m_panel_manager.add_panel<FileBrowserPanel>();
    m_panel_manager.add_panel<SceneManagerPanel>();
    m_panel_manager.add_panel<AssetPreviewPanel>();
    m_panel_manager.add_panel<BuildSettingsPanel>();
    m_panel_manager.add_panel<ProjectSettingsPanel>(*m_project_manager);
    m_panel_manager.add_panel<PrefabEditorPanel>(m_context);

    // Wire up menu bar callbacks
    m_panel_manager.menu_callbacks.new_scene = [this]() {
        confirm_discard_or_save([this]() { new_scene(); });
    };
    m_panel_manager.menu_callbacks.save_scene = [this]() { save_scene(); };
    m_panel_manager.menu_callbacks.save_scene_as = [this]() { save_scene_as(); };
    m_panel_manager.menu_callbacks.show_project_hub = [this]() {
        confirm_all_unsaved([this]() {
            if (auto* hub = m_panel_manager.get_panel<ProjectHubPanel>()) {
                hub->set_visible(true);
            }
        });
    };
    m_panel_manager.menu_callbacks.exit = [this]() { request_exit(); };
    m_panel_manager.menu_callbacks.undo = [this]() { m_context.undo(); };
    m_panel_manager.menu_callbacks.redo = [this]() { m_context.redo(); };
    m_panel_manager.menu_callbacks.cut = [this]() {
        m_context.copy_selection();
        auto selection = m_context.selection();
        for (auto entity : selection) {
            if (m_scene_registry.valid(entity)) {
                auto cmd = std::make_unique<DeleteEntityCommand>(&m_scene_registry, &m_context, entity);
                m_context.execute_command(std::move(cmd));
            }
        }
    };
    m_panel_manager.menu_callbacks.copy = [this]() { m_context.copy_selection(); };
    m_panel_manager.menu_callbacks.paste = [this]() { m_context.paste(); };
    m_panel_manager.menu_callbacks.duplicate = [this]() { m_context.duplicate_selection(); };
    m_panel_manager.menu_callbacks.delete_selected = [this]() {
        auto selection = m_context.selection();
        for (auto entity : selection) {
            if (m_scene_registry.valid(entity)) {
                auto cmd = std::make_unique<DeleteEntityCommand>(&m_scene_registry, &m_context, entity);
                m_context.execute_command(std::move(cmd));
            }
        }
    };

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
        if (auto* p = m_panel_manager.get_panel<GamePanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<FileBrowserPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<SceneManagerPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<AssetPreviewPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<BuildSettingsPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<ProjectSettingsPanel>()) p->set_visible(false);
        if (auto* p = m_panel_manager.get_panel<PrefabEditorPanel>()) p->set_visible(false);
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

    // Update pixel grid loader (loads .pxg files for PixelGridComponents)
    m_context.pixel_grid_loader().update(m_context.registry());

    // Update world transforms from hierarchy (parent-child propagation)
    update_world_transforms(*m_context.registry());

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

    render_unsaved_changes_dialog();

    // Render prefab unsaved dialog at top level (outside panel Begin/End)
    if (auto* prefab = m_panel_manager.get_panel<PrefabEditorPanel>()) {
        prefab->render_unsaved_dialog();
    }

    end_frame();
}

void EditorApplication::request_exit() {
    confirm_all_unsaved([this]() {
        m_should_exit = true;
    });
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
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Set up ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;

    // When viewports are enabled, tweak style so platform windows blend seamlessly
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Slightly darker background
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);

    // Merge Font Awesome icon font into the default font atlas
    {
        ImFontConfig config;
        config.MergeMode = true;
        config.PixelSnapH = true;
        config.GlyphMinAdvanceX = 13.0f;
        static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        io.Fonts->AddFontDefault();
        io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", 13.0f, &config, icon_ranges);
    }

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

    // Multi-viewport support: update and render platform windows
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_context);
    }
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
            if (ImGui::Button(ICON_FA_PLAY)) {
                m_runtime.play(m_context.scene_settings());
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play");
        } else if (is_paused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
            if (ImGui::Button(ICON_FA_PLAY)) {
                m_runtime.resume();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Resume");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
            ImGui::Button(ICON_FA_PLAY);
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Pause button
        ImGui::BeginDisabled(!is_playing || is_paused);
        if (ImGui::Button(ICON_FA_PAUSE)) {
            m_runtime.pause();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause");
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Stop button
        ImGui::BeginDisabled(!is_playing);
        if (ImGui::Button(ICON_FA_STOP)) {
            m_runtime.stop();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Step button (only when paused)
        ImGui::BeginDisabled(!is_paused);
        if (ImGui::Button(ICON_FA_FORWARD_STEP)) {
            m_runtime.step_frame();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step Frame");
        ImGui::EndDisabled();

        // Show play mode info
        if (is_playing) {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
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
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Gizmo mode buttons
        auto* viewport = m_panel_manager.get_panel<ViewportPanel>();
        GizmoMode current_mode = viewport ? viewport->gizmo_renderer().mode() : GizmoMode::Translate;

        ImVec4 active_color(0.3f, 0.5f, 0.8f, 1.0f);
        ImVec4 active_hovered(0.4f, 0.6f, 0.9f, 1.0f);

        // Move button (W)
        bool is_translate = (current_mode == GizmoMode::Translate);
        if (is_translate) {
            ImGui::PushStyleColor(ImGuiCol_Button, active_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
        }
        if (ImGui::Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT)) {
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
        if (ImGui::Button(ICON_FA_ROTATE)) {
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
        if (ImGui::Button(ICON_FA_EXPAND)) {
            if (viewport) viewport->gizmo_renderer().set_mode(GizmoMode::Scale);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale Tool (R)");
        if (is_scale) ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Coordinate space toggle (Local/World)
        bool is_local = m_context.is_local_space();
        if (ImGui::Button(is_local ? ICON_FA_CUBE " Local" : ICON_FA_GLOBE " World")) {
            m_context.set_local_space(!is_local);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle coordinate space for gizmos");

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Grid/Snap toggles
        bool grid_visible = m_context.is_grid_visible();
        if (ImGui::Checkbox(ICON_FA_BORDER_ALL " Grid", &grid_visible)) {
            m_context.set_grid_visible(grid_visible);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle grid visibility (G)");

        ImGui::SameLine();

        bool snap_enabled = m_context.is_snap_enabled();
        if (ImGui::Checkbox(ICON_FA_MAGNET " Snap", &snap_enabled)) {
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

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Gizmo visibility / debug overlays dropdown
        if (ImGui::Button(ICON_FA_EYE " Gizmos")) {
            ImGui::OpenPopup("GizmoVisibilityPopup");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Configure debug overlay visibility");

        if (ImGui::BeginPopup("GizmoVisibilityPopup")) {
            auto& vis = m_context.gizmo_visibility();

            ImGui::TextUnformatted("Debug Overlays");
            ImGui::Separator();

            if (ImGui::BeginTable("##GizmoVisTable", 4, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Overlay", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Off");
                ImGui::TableSetupColumn("Sel");
                ImGui::TableSetupColumn("All");
                ImGui::TableHeadersRow();

                auto visibility_row = [](const char* label, GizmoVisibility& v) {
                    ImGui::PushID(label);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(label);
                    ImGui::TableNextColumn();
                    if (ImGui::RadioButton("##none", v == GizmoVisibility::None))
                        v = GizmoVisibility::None;
                    ImGui::TableNextColumn();
                    if (ImGui::RadioButton("##sel", v == GizmoVisibility::SelectedOnly))
                        v = GizmoVisibility::SelectedOnly;
                    ImGui::TableNextColumn();
                    if (ImGui::RadioButton("##all", v == GizmoVisibility::All))
                        v = GizmoVisibility::All;
                    ImGui::PopID();
                };

                visibility_row("Colliders", vis.colliders);
                visibility_row("Terrain Colliders", vis.terrain_colliders);
                visibility_row("Object Origin", vis.object_origin);
                visibility_row("Object Name", vis.object_name);
                visibility_row("Camera Bounds", vis.camera_bounds);
                visibility_row("Rigidbody Velocity", vis.rigidbody_velocity);
                visibility_row("Pixel Grid Bounds", vis.pixel_grid_bounds);
                visibility_row("Parent-Child Links", vis.parent_child_links);

                ImGui::EndTable();
            }

            ImGui::EndPopup();
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
            m_runtime.play(m_context.scene_settings());
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

        // Connect file browser open to scene loading
        file_browser->set_file_opened_callback([this](const std::string& path) {
            on_file_opened(path);
        });
    }
    if (auto* p = m_panel_manager.get_panel<SceneManagerPanel>()) p->set_visible(true);
    if (asset_preview) asset_preview->set_visible(true);

    // Game panel: visible by default, tabbed with Viewport
    if (auto* p = m_panel_manager.get_panel<GamePanel>()) p->set_visible(true);

    // Configure build settings panel
    if (auto* build_settings = m_panel_manager.get_panel<BuildSettingsPanel>()) {
        build_settings->set_visible(false);  // Hidden by default, accessible via menu
        build_settings->set_project_path(m_project_manager->project_path());
        build_settings->set_engine_paths(m_engine_src_path, engine_build_path);
    }

    // Prefab editor: hidden until a .prefab file is opened
    if (auto* p = m_panel_manager.get_panel<PrefabEditorPanel>()) p->set_visible(false);

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
    auto& transform = m_scene_registry.get<engine::Transform>(sample);
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
            path = m_project_manager->project_path() + "/Assets/Untitled.scene";
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

void EditorApplication::save_scene_as() {
    std::string initial_dir;
    if (has_project()) {
        initial_dir = m_project_manager->project_path() + "/Assets";
    }

    std::string path = engine::platform::save_file_dialog(
        "Save Scene As",
        {{"Scene Files (*.scene)", "*.scene"}, {"All Files", "*.*"}},
        "scene",
        initial_dir);

    if (!path.empty()) {
        SceneSerializer serializer(m_scene_registry);
        if (serializer.save(path)) {
            m_context.set_current_scene_path(path);
            m_context.clear_dirty();
            m_context.history().mark_saved();
            engine::Logger::instance().info("Editor", "Scene saved as: %s", path.c_str());
        } else {
            engine::Logger::instance().error("Editor", "Failed to save scene: %s", serializer.last_error().c_str());
        }
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

void EditorApplication::on_file_opened(const std::string& path) {
    // Check file extension
    std::filesystem::path fs_path(path);
    std::string ext = fs_path.extension().string();

    if (ext == ".scene") {
        confirm_discard_or_save([this, path]() {
            load_scene(path);
        });
    } else if (ext == ".pxg") {
        launch_pixart(path);
    } else if (ext == ".prefab") {
        if (auto* prefab_editor = m_panel_manager.get_panel<PrefabEditorPanel>()) {
            // open_prefab internally checks dirty state and prompts if needed
            prefab_editor->open_prefab(path);
        }
    }
}

void EditorApplication::launch_pixart(const std::string& file_path) {
    std::filesystem::path exe_dir = std::filesystem::current_path();
#ifdef _WIN32
    std::filesystem::path pixart_path = exe_dir / "pixart.exe";
#else
    std::filesystem::path pixart_path = exe_dir / "pixart";
#endif

    if (!std::filesystem::exists(pixart_path)) {
        engine::Logger::instance().error("Editor", "pixart not found at: %s", pixart_path.string().c_str());
        return;
    }

    std::string args = "\"" + file_path + "\"";
    if (engine::platform::launch_detached(pixart_path.string(), args)) {
        engine::Logger::instance().info("Editor", "Launched PixArt for: %s", file_path.c_str());
    } else {
        engine::Logger::instance().error("Editor", "Failed to launch pixart");
    }
}

void EditorApplication::confirm_discard_or_save(std::function<void()> action) {
    if (!m_context.is_dirty()) {
        // Scene is clean, execute immediately
        action();
        return;
    }

    // Scene has unsaved changes, show confirmation dialog
    m_pending_action = std::move(action);
    m_show_unsaved_dialog = true;
}

void EditorApplication::confirm_all_unsaved(std::function<void()> action) {
    // Chain: first check prefab dirty, then check scene dirty, then execute
    auto check_scene_then_act = [this, action = std::move(action)]() {
        confirm_discard_or_save(std::move(action));
    };

    auto* prefab_editor = m_panel_manager.get_panel<PrefabEditorPanel>();
    if (prefab_editor && prefab_editor->has_unsaved_changes()) {
        prefab_editor->confirm_prefab_discard_or_save(std::move(check_scene_then_act));
    } else {
        check_scene_then_act();
    }
}

void EditorApplication::render_unsaved_changes_dialog() {
    if (m_show_unsaved_dialog) {
        ImGui::OpenPopup("Unsaved Changes");
        m_show_unsaved_dialog = false;
    }

    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The current scene has unsaved changes.");
        ImGui::Text("Do you want to save before continuing?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            save_scene();
            if (m_pending_action) {
                m_pending_action();
                m_pending_action = nullptr;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
            if (m_pending_action) {
                m_pending_action();
                m_pending_action = nullptr;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_pending_action = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace editor

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
#include "editor/panels/ScreenPanel.h"
#include "editor/panels/ProfilerPanel.h"
#include "editor/panels/MaterialEditorPanel.h"
#include "editor/panels/PixArtPanel.h"
#include "editor/panels/AnimatorPanel.h"
#include "editor/panels/AnimationPanel.h"
#include "editor/commands/EntityCommands.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/render/Camera2D.h"
#include "editor/gizmos/Gizmo.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "editor/ui/UnsavedDialog.h"

#include "engine/core/Engine.h"
#include "engine/core/Logger.h"
#include "engine/reflection/ReflectionInit.h"
#include "engine/animation/AnimationInit.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/platform/Window.h"
#include "engine/platform/Input.h"

#include "engine/platform/PlatformUtils.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>

namespace editor {

EditorApplication::EditorApplication()
    : m_project_manager(std::make_unique<ProjectManager>())
{
    m_context.set_registry(&m_scene_registry);

    m_runtime.init(&m_scene_registry, &m_script_manager);
    m_runtime.set_pixel_grid_loader(&m_context.pixel_grid_loader());

    m_context.set_runtime(&m_runtime);
}

EditorApplication::~EditorApplication() = default;

bool EditorApplication::on_init(engine::Engine& engine) {
    engine::reflection::init_engine_reflections();
    engine::animation::init_animation_property_resolver();

    m_runtime.set_engine(&engine);

    init_component_type_registry();

    std::filesystem::path exe_dir = engine::platform::executable_directory();

    std::filesystem::path categories_path = exe_dir / "assets" / "categories";
    m_category_library.ensure_empty_category();
    m_category_library.load_from_directory(categories_path.string(), true);

    m_runtime.set_category_library(&m_category_library);

    auto& mat_registry = engine::simulation::MaterialLibraryRegistry::instance();
    auto* mat_lib = mat_registry.get_or_create_library("default");
    mat_lib->set_category_library(&m_category_library);

    std::filesystem::path materials_path = exe_dir / "assets" / "materials";
    if (!mat_lib->load_from_directory(materials_path.string())) {
        engine::Logger::instance().error("EditorApp", "Failed to load default material library from: %s", materials_path.string().c_str());
    }

    engine.scenes().push(std::make_unique<EditorScene>());

    init_imgui(engine);

    m_panel_manager.init();

    m_panel_manager.add_panel<ProjectHubPanel>(*m_project_manager, *this);
    m_panel_manager.add_panel<ConsolePanel>();
    m_panel_manager.add_panel<HierarchyPanel>(m_context);
    m_panel_manager.add_panel<InspectorPanel>(m_context);
    m_panel_manager.add_panel<ViewportPanel>(m_context);
    m_panel_manager.add_panel<ScreenPanel>(m_context);
    m_panel_manager.add_panel<GamePanel>(m_context);
    m_panel_manager.add_panel<FileBrowserPanel>();
    m_panel_manager.add_panel<SceneManagerPanel>();
    m_panel_manager.add_panel<AssetPreviewPanel>();
    m_panel_manager.add_panel<BuildSettingsPanel>();
    m_panel_manager.add_panel<ProjectSettingsPanel>(*m_project_manager);
    m_panel_manager.add_panel<PrefabEditorPanel>(m_context);

    auto* material_editor = m_panel_manager.add_panel<MaterialEditorPanel>(m_context);
    material_editor->set_library(mat_lib);
    material_editor->set_category_library(&m_category_library);
    material_editor->set_visible(false);

    if (auto* file_browser = m_panel_manager.get_panel<FileBrowserPanel>()) {
        file_browser->set_material_create_callback([material_editor]() {
            material_editor->open_new_material_dialog();
        });
        file_browser->set_category_create_callback([material_editor]() {
            material_editor->open_new_category_dialog();
        });
    }

    auto* profiler_panel = m_panel_manager.add_panel<ProfilerPanel>(engine, m_context);
    profiler_panel->set_gpu_profiler(engine.gpu_profiler());
    profiler_panel->set_visible(false);

    auto* pixart_panel = m_panel_manager.add_panel<PixArtPanel>(m_context);
    pixart_panel->set_visible(false);

    auto* animator_panel = m_panel_manager.add_panel<AnimatorPanel>(m_context);
    animator_panel->set_visible(false);

    auto* animation_panel = m_panel_manager.add_panel<AnimationPanel>(m_context);
    animation_panel->set_visible(false);

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
        delete_selection();
    };
    m_panel_manager.menu_callbacks.copy = [this]() { m_context.copy_selection(); };
    m_panel_manager.menu_callbacks.paste = [this]() { m_context.paste(); };
    m_panel_manager.menu_callbacks.duplicate = [this]() { m_context.duplicate_selection(); };
    m_panel_manager.menu_callbacks.delete_selected = [this]() {
        delete_selection();
    };
    m_panel_manager.menu_callbacks.reset_layout = [this]() {
        m_context.viewport().camera.reset();
        if (auto* prefab_panel = m_panel_manager.get_panel<PrefabEditorPanel>()) {
            prefab_panel->reset_camera();
        }
        if (has_project()) {
            m_panel_manager.show_editor_panels();
        }
    };

    if (!has_project()) {
        auto* hub = m_panel_manager.get_panel<ProjectHubPanel>();
        if (hub) {
            hub->set_visible(true);
            hub->set_closable(false);
        }

        m_panel_manager.hide_editor_panels();
    }

    return true;
}

void EditorApplication::on_shutdown(engine::Engine& /*engine*/) {
    m_context.shutdown_asset_registry();
    m_script_manager.shutdown();
    m_panel_manager.shutdown();
    shutdown_imgui();
}

void EditorApplication::on_update(engine::Engine& engine, float dt) {
    if (engine.window().focus_just_gained() && has_project()) {
        m_context.rescan_assets_for_external_changes();
        m_context.refresh_file_browser();
    }

    handle_shortcuts(engine);

    m_script_manager.update();

    if (auto* build_settings = m_panel_manager.get_panel<BuildSettingsPanel>()) {
        build_settings->update();
    }

    m_context.pixel_grid_loader().update(m_context.scene_registry());

    update_world_transforms(*m_context.registry());

    update_screen_rects(*m_context.registry(), 1920.0f, 1080.0f);

    if (m_runtime.is_playing()) {
        m_runtime.update(dt);
    }
}

void EditorApplication::on_render(engine::Engine& engine) {
    begin_frame();

    m_panel_manager.render_menu_bar();

    if (has_project()) {
        m_toolbar.render();
    } else {
        m_panel_manager.set_toolbar_height(0.0f);
    }

    m_panel_manager.render();

    render_unsaved_changes_dialog();

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

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);

    {
        io.Fonts->AddFontDefault();

        // Try to load Font Awesome icons (gracefully skip if not found)
        std::filesystem::path exe_dir = engine::platform::executable_directory();
        std::filesystem::path icon_font_path = exe_dir / "assets" / "fonts" / "fa-solid-900.ttf";
        if (std::filesystem::exists(icon_font_path)) {
            ImFontConfig config;
            config.MergeMode = true;
            config.PixelSnapH = true;
            config.GlyphMinAdvanceX = 13.0f;
            static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            io.Fonts->AddFontFromFileTTF(icon_font_path.string().c_str(), 13.0f, &config, icon_ranges);
        } else {
            engine::Logger::instance().warning("Editor", "Font Awesome icons not found at: %s", icon_font_path.string().c_str());
        }
    }

    m_imgui_backend = engine::platform::create_imgui_backend();
    if (!m_imgui_backend || !m_imgui_backend->init(engine.window())) {
        engine::Logger::instance().error("Editor", "Failed to initialize ImGui backend");
        return;
    }

    m_imgui_initialized = true;
}

void EditorApplication::shutdown_imgui() {
    if (m_imgui_initialized) {
        if (m_imgui_backend) {
            m_imgui_backend->shutdown();
            m_imgui_backend.reset();
        }
        ImGui::DestroyContext();
        m_imgui_initialized = false;
    }
}

void EditorApplication::begin_frame() {
    if (m_imgui_backend) {
        m_imgui_backend->new_frame();
    }
    ImGui::NewFrame();
}

void EditorApplication::end_frame() {
    ImGui::Render();

    if (m_imgui_backend) {
        m_imgui_backend->render_draw_data();
        m_imgui_backend->update_platform_windows();
    }
}

void EditorApplication::handle_shortcuts(engine::Engine& engine) {
    using engine::platform::KeyCode;
    auto& input = engine.input();

    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::S)) {
        save_scene();
    }

    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::Z)) {
        m_context.undo();
    }

    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::Y)) {
        m_context.redo();
    }
    if (input.is_held(KeyCode::LeftCtrl) && input.is_held(KeyCode::LeftShift) && input.is_pressed(KeyCode::Z)) {
        m_context.redo();
    }

    if (input.is_pressed(KeyCode::Delete)) {
        delete_selection();
    }

    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::C)) {
        m_context.copy_selection();
    }

    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::V)) {
        m_context.paste();
    }

    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::D)) {
        m_context.duplicate_selection();
    }

    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::P)) {
        if (!m_runtime.is_playing()) {
            m_runtime.play(m_context.scene_settings());
        } else if (m_runtime.is_paused()) {
            m_runtime.resume();
        } else {
            m_runtime.pause();
        }
    }

    if (input.is_pressed(KeyCode::Escape)) {
        if (m_runtime.is_playing()) {
            m_runtime.stop();
        }
    }

    if (input.is_pressed(KeyCode::F5)) {
        if (m_runtime.is_paused()) {
            m_runtime.step_frame();
        }
    }

    if (input.is_held(KeyCode::LeftCtrl) && input.is_pressed(KeyCode::B)) {
        rebuild_scripts();
    }

    if (!ImGui::GetIO().WantTextInput) {
        auto* viewport = m_panel_manager.get_panel<ViewportPanel>();
        if (viewport) {
            if (input.is_pressed(KeyCode::W)) {
                viewport->gizmo_renderer().set_mode(GizmoMode::Translate);
            }
            if (input.is_pressed(KeyCode::E)) {
                viewport->gizmo_renderer().set_mode(GizmoMode::Rotate);
            }
            if (input.is_pressed(KeyCode::R)) {
                viewport->gizmo_renderer().set_mode(GizmoMode::Scale);
            }
        }

        if (input.is_pressed(KeyCode::F)) {
            m_context.focus_on_selection();
        }

        if (input.is_pressed(KeyCode::G)) {
            m_context.viewport().grid_visible = !m_context.viewport().grid_visible;
        }
    }
}

void EditorApplication::on_project_loaded() {
    engine::Logger::instance().info("Editor", "Project loaded: %s", m_project_manager->project_info().name.c_str());

    m_context.scene_state().set_project_path(m_project_manager->project_path());

    load_project_assets();

    // Initialize asset registry to track asset GUIDs
    m_context.init_asset_registry(m_project_manager->project_path());

    std::filesystem::path exe_dir = std::filesystem::current_path();

    std::filesystem::path build_root;
    if (std::filesystem::exists(exe_dir / "_deps")) {
        build_root = exe_dir;
    } else {
        build_root = exe_dir.parent_path();
    }
    std::string engine_build_path = build_root.string();

    if (m_engine_src_path.empty()) {
        std::filesystem::path src_path = build_root / "../src";

        std::error_code ec;
        std::filesystem::path canonical = std::filesystem::canonical(src_path, ec);
        if (!ec) {
            m_engine_src_path = canonical.string();
        } else {
            m_engine_src_path = (build_root / "../src").string();
        }
        engine::Logger::instance().info("Editor", "Engine source path: %s", m_engine_src_path.c_str());
        engine::Logger::instance().info("Editor", "Engine build path: %s", engine_build_path.c_str());
    }
    m_script_manager.init(m_project_manager->project_path(), m_engine_src_path, engine_build_path);
    m_context.set_script_manager(&m_script_manager);

    auto runtime_assets_path = std::filesystem::path(m_project_manager->project_path()) / "Assets";
    m_runtime.set_project_assets_path(runtime_assets_path.string());

    if (auto* p = m_panel_manager.get_panel<ConsolePanel>()) p->set_visible(true);
    if (auto* p = m_panel_manager.get_panel<HierarchyPanel>()) p->set_visible(true);
    if (auto* p = m_panel_manager.get_panel<InspectorPanel>()) p->set_visible(true);
    if (auto* p = m_panel_manager.get_panel<ViewportPanel>()) p->set_visible(true);
    if (auto* p = m_panel_manager.get_panel<ScreenPanel>()) p->set_visible(true);
    auto* asset_preview = m_panel_manager.get_panel<AssetPreviewPanel>();
    if (auto* file_browser = m_panel_manager.get_panel<FileBrowserPanel>()) {
        file_browser->set_visible(true);
        file_browser->set_editor_context(&m_context);
        auto assets_path = std::filesystem::path(m_project_manager->project_path()) / "Assets";
        std::filesystem::create_directories(assets_path);
        file_browser->set_root(assets_path.string());

        if (asset_preview) {
            file_browser->set_file_selected_callback([asset_preview](const std::string& path) {
                asset_preview->set_asset(path);
            });
        }

        file_browser->set_file_opened_callback([this](const std::string& path) {
            on_file_opened(path);
        });

        m_context.set_file_browser_refresh_callback([file_browser]() {
            file_browser->refresh();
        });
    }
    if (auto* p = m_panel_manager.get_panel<SceneManagerPanel>()) p->set_visible(true);
    if (asset_preview) asset_preview->set_visible(true);

    if (auto* p = m_panel_manager.get_panel<GamePanel>()) p->set_visible(true);

    if (auto* build_settings = m_panel_manager.get_panel<BuildSettingsPanel>()) {
        build_settings->set_visible(false);
        build_settings->set_project_path(m_project_manager->project_path());
        build_settings->set_engine_paths(m_engine_src_path, engine_build_path);
    }

    if (auto* p = m_panel_manager.get_panel<PrefabEditorPanel>()) p->set_visible(false);

    if (auto* hub = m_panel_manager.get_panel<ProjectHubPanel>()) {
        hub->set_visible(false);
    }

    new_scene();
}

void EditorApplication::rebuild_scripts() {
    m_script_manager.rebuild();
}

void EditorApplication::load_project_assets() {
    if (!has_project()) return;

    namespace fs = std::filesystem;
    fs::path assets_root = fs::path(m_project_manager->project_path()) / "Assets";

    if (!fs::exists(assets_root)) return;

    // Load project categories (custom .phys files)
    int category_count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(assets_root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".phys") {
            if (m_category_library.load_category(entry.path().string(), false)) {
                category_count++;
            }
        }
    }
    if (category_count > 0) {
        engine::Logger::instance().info("Editor", "Loaded %d project categories", category_count);
    }

    // Reload material library with project materials
    auto& mat_registry = engine::simulation::MaterialLibraryRegistry::instance();
    auto* mat_lib = mat_registry.get_or_create_library("default");
    if (mat_lib) {
        mat_lib->set_category_library(&m_category_library);

        mat_lib->clear();

        std::filesystem::path exe_dir = engine::platform::executable_directory();
        mat_lib->load_from_directory((exe_dir / "assets" / "materials").string());

        int material_count = 0;
        for (const auto& entry : fs::recursive_directory_iterator(assets_root)) {
            if (entry.is_regular_file() && entry.path().extension() == ".material") {
                if (mat_lib->load_material_file(entry.path().string())) {
                    material_count++;
                }
            }
        }
        if (material_count > 0) {
            engine::Logger::instance().info("Editor", "Loaded %d project materials", material_count);
        }
    }
}

void EditorApplication::new_scene() {
    m_scene_registry.clear();
    m_context.selection().clear_selection();
    m_context.scene_state().clear_dirty();
    m_context.scene_state().set_scene_path("");

    auto camera = create_entity(m_scene_registry, "Main Camera");
    m_scene_registry.emplace<engine::render::Camera2D>(camera);

    auto sample = create_entity(m_scene_registry, "Sample Entity");
    auto& transform = m_scene_registry.get<engine::Transform>(sample);
    transform.x = 100.0f;
    transform.y = 100.0f;

    engine::Logger::instance().info("Editor", "New scene created");

    m_context.history().clear();
}

void EditorApplication::save_scene() {
    std::string path = m_context.scene_state().scene_path();

    if (path.empty()) {
        if (has_project()) {
            path = m_project_manager->project_path() + "/Assets/Untitled.scene";
        } else {
            engine::Logger::instance().warning("Editor", "No project loaded, cannot save scene");
            return;
        }
    }

    SceneSerializer serializer(m_scene_registry);
    if (serializer.save(path)) {
        m_context.scene_state().set_scene_path(path);
        m_context.scene_state().clear_dirty();
        m_context.history().mark_saved();
        engine::Logger::instance().info("Editor", "Scene saved: %s", path.c_str());
    } else {
        engine::Logger::instance().error("Editor", "Failed to save scene: %s", serializer.last_error().c_str());
    }
}

void EditorApplication::save_scene_as() {
    std::string initial_dir;
    if (has_project()) {
        std::filesystem::path assets_dir = std::filesystem::path(m_project_manager->project_path()) / "Assets";
        if (std::filesystem::exists(assets_dir)) {
            initial_dir = assets_dir.string();
        } else {
            initial_dir = m_project_manager->project_path();
        }
    }

    std::string path = engine::platform::save_file_dialog(
        "Save Scene As",
        {{"Scene Files (*.scene)", "*.scene"}, {"All Files", "*.*"}},
        "scene",
        initial_dir);

    if (!path.empty()) {
        SceneSerializer serializer(m_scene_registry);
        if (serializer.save(path)) {
            m_context.scene_state().set_scene_path(path);
            m_context.scene_state().clear_dirty();
            m_context.history().mark_saved();
            engine::Logger::instance().info("Editor", "Scene saved as: %s", path.c_str());
        } else {
            engine::Logger::instance().error("Editor", "Failed to save scene: %s", serializer.last_error().c_str());
        }
    }
}

bool EditorApplication::load_scene(const std::string& path) {
    SceneSerializer serializer(m_scene_registry);

    m_scene_registry.clear();
    m_context.selection().clear_selection();
    m_context.history().clear();

    if (serializer.load(path)) {
        m_context.scene_state().set_scene_path(path);
        m_context.scene_state().clear_dirty();
        m_context.history().mark_saved();
        engine::Logger::instance().info("Editor", "Scene loaded: %s", path.c_str());
        return true;
    } else {
        engine::Logger::instance().error("Editor", "Failed to load scene: %s", serializer.last_error().c_str());
        new_scene();
        return false;
    }
}

void EditorApplication::on_file_opened(const std::string& path) {
    std::filesystem::path fs_path(path);
    std::string ext = fs_path.extension().string();

    if (ext == ".scene") {
        confirm_discard_or_save([this, path]() {
            load_scene(path);
        });
    } else if (ext == ".pxg") {
        if (auto* pixart = m_panel_manager.get_panel<PixArtPanel>()) {
            pixart->open_file(path);
            pixart->set_visible(true);
        }
    } else if (ext == ".prefab") {
        if (auto* prefab_editor = m_panel_manager.get_panel<PrefabEditorPanel>()) {
            prefab_editor->open_prefab(path);
        }
    } else if (ext == ".material") {
        if (auto* material_editor = m_panel_manager.get_panel<MaterialEditorPanel>()) {
            material_editor->select_material_by_path(path);
        }
    } else if (ext == ".phys") {
        if (auto* material_editor = m_panel_manager.get_panel<MaterialEditorPanel>()) {
            material_editor->select_category_by_path(path);
        }
    } else if (ext == ".anim") {
        if (auto* animation_panel = m_panel_manager.get_panel<AnimationPanel>()) {
            animation_panel->open_clip(path);
            animation_panel->set_visible(true);
        }
    } else if (ext == ".animstate") {
        if (auto* animator_panel = m_panel_manager.get_panel<AnimatorPanel>()) {
            animator_panel->open_controller(path);
            animator_panel->set_visible(true);
        }
    }
}


void EditorApplication::delete_selection() {
    auto selection = m_context.selection().selection();
    for (auto entity : selection) {
        if (m_scene_registry.valid(entity)) {
            auto cmd = std::make_unique<DeleteEntityCommand>(&m_scene_registry, &m_context, entity);
            m_context.execute_command(std::move(cmd));
        }
    }
}

void EditorApplication::confirm_discard_or_save(std::function<void()> action) {
    if (!m_context.scene_state().is_dirty()) {
        action();
        return;
    }

    m_pending_action = std::move(action);
    m_show_unsaved_dialog = true;
}

void EditorApplication::confirm_all_unsaved(std::function<void()> action) {
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

    auto result = ui::render_unsaved_popup("Unsaved Changes",
                                            "The current scene has unsaved changes.");
    switch (result) {
        case ui::UnsavedAction::Save:
            save_scene();
            if (m_pending_action) { auto a = std::move(m_pending_action); m_pending_action = nullptr; a(); }
            break;
        case ui::UnsavedAction::DontSave:
            if (m_pending_action) { auto a = std::move(m_pending_action); m_pending_action = nullptr; a(); }
            break;
        case ui::UnsavedAction::Cancel:
            m_pending_action = nullptr;
            break;
        default: break;
    }
}

}

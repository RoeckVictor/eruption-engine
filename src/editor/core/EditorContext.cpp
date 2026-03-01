#include "EditorContext.h"
#include "EditorComponents.h"
#include "SimulationPlayback.h"
#include "editor/commands/Command.h"
#include "editor/commands/EntityCommands.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/Logger.h"
#include "engine/core/Transform.h"
#include "engine/platform/PlatformUtils.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace editor {

namespace {

/// Compare two paths for equality (handles platform differences)
bool paths_equal(const std::string& a, const std::string& b) {
    return engine::platform::normalize_path_for_comparison(a) ==
           engine::platform::normalize_path_for_comparison(b);
}

} // anonymous namespace

EditorContext::EditorContext() = default;
EditorContext::~EditorContext() = default;

void EditorContext::set_registry(entt::registry* registry) {
    m_registry = registry;
    // Clear selection when registry changes
    m_selection.clear_selection();
}

void EditorContext::set_editing_override(const EditingOverride& override) {
    m_editing_override = override;

    // Apply to SelectionContext
    m_selection.set_selection_override(override.selection);

    // Apply to SceneStateContext
    if (override.mark_dirty) {
        m_scene_state.set_dirty_override(override.mark_dirty);
    } else {
        m_scene_state.clear_dirty_override();
    }
}

void EditorContext::clear_editing_override() {
    if (m_editing_override.registry) {
        m_editing_override = {};
        m_selection.clear_selection_override();
        m_scene_state.clear_dirty_override();
    }
}

void EditorContext::execute_command(std::unique_ptr<Command> cmd) {
    if (cmd) {
        m_history.execute(std::move(cmd));
        m_scene_state.mark_dirty();
    }
}

void EditorContext::undo() {
    if (m_history.can_undo()) {
        m_history.undo();
        m_scene_state.mark_dirty();
    }
}

void EditorContext::redo() {
    if (m_history.can_redo()) {
        m_history.redo();
        m_scene_state.mark_dirty();
    }
}

void EditorContext::copy_selection() {
    auto* reg = registry();
    const auto& sel = m_selection.selection();
    if (!reg || sel.empty()) {
        return;
    }

    SceneSerializer serializer(*reg);
    nlohmann::json json = serializer.serialize_entities(sel);
    m_clipboard.set_entity_clipboard(json.dump());

    engine::Logger::instance().info("Editor", "Copied %zu entities to clipboard", sel.size());
}

void EditorContext::paste() {
    auto* reg = registry();
    if (!reg || !m_clipboard.has_entity_clipboard()) {
        return;
    }

    auto cmd = std::make_unique<PasteEntitiesCommand>(reg, this, m_clipboard.entity_clipboard());
    execute_command(std::move(cmd));
}

void EditorContext::duplicate_selection() {
    auto* reg = registry();
    const auto& sel = m_selection.selection();
    if (!reg || sel.empty()) {
        return;
    }

    copy_selection();
    paste();
}

void EditorContext::focus_on_selection() {
    auto* reg = registry();
    const auto& sel = m_selection.selection();
    if (!reg || sel.empty()) {
        return;
    }

    // Calculate bounding box of all selected entities
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    bool has_transform = false;

    for (auto entity : sel) {
        if (reg->valid(entity) && reg->all_of<engine::Transform>(entity)) {
            const auto& transform = reg->get<engine::Transform>(entity);
            min_x = std::min(min_x, transform.x);
            min_y = std::min(min_y, transform.y);
            max_x = std::max(max_x, transform.x);
            max_y = std::max(max_y, transform.y);
            has_transform = true;
        }
    }

    if (has_transform) {
        // Center camera on the bounding box center
        m_viewport.camera.x = (min_x + max_x) * 0.5f;
        m_viewport.camera.y = (min_y + max_y) * 0.5f;

        engine::Logger::instance().info("Editor", "Focused camera on selection at (%.1f, %.1f)",
                                         m_viewport.camera.x, m_viewport.camera.y);
    }
}

void EditorContext::sync_prefab_to_instances(const std::string& prefab_path) {
    if (!m_registry || prefab_path.empty()) return;

    // Load the updated prefab data once
    entt::registry temp_registry;
    SceneSerializer temp_serializer(temp_registry);
    entt::entity prefab_root = temp_serializer.load_prefab(prefab_path);
    if (prefab_root == entt::null) {
        engine::Logger::instance().error("EditorContext", "Failed to load prefab for sync: %s", prefab_path.c_str());
        return;
    }

    // Find and update all instances in the current scene
    std::vector<entt::entity> instances;
    auto view = m_registry->view<EntityInfo>();
    for (auto entity : view) {
        const auto& info = view.get<EntityInfo>(entity);
        if (info.is_prefab_instance && paths_equal(info.prefab_path, prefab_path)) {
            instances.push_back(entity);
        }
    }

    if (!instances.empty()) {
        SceneSerializer serializer(*m_registry);
        for (auto instance : instances) {
            if (!m_registry->valid(instance)) continue;

            // sync_entity_from_prefab preserves position (x, y) but syncs rotation, scale, and other components
            serializer.sync_entity_from_prefab(instance, temp_registry, prefab_root);
        }

        m_scene_state.mark_dirty();
        engine::Logger::instance().info("EditorContext", "Synced %zu prefab instance(s) in current scene from: %s",
                                         instances.size(), prefab_path.c_str());
    }

    // Also update all other scene files in the project
    if (!m_scene_state.project_path().empty()) {
        sync_prefab_to_project_scenes(prefab_path, temp_registry, prefab_root);
    }
}

void EditorContext::sync_prefab_to_project_scenes(const std::string& prefab_path,
                                                   entt::registry& prefab_registry,
                                                   entt::entity prefab_root) {
    namespace fs = std::filesystem;

    const auto& project_path = m_scene_state.project_path();
    const auto& scene_path = m_scene_state.scene_path();

    if (project_path.empty()) return;

    int scenes_updated = 0;

    // Find all .scene files in the project
    try {
        for (const auto& entry : fs::recursive_directory_iterator(project_path)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".scene") continue;

            // Skip the currently open scene - it's already updated in memory
            if (paths_equal(entry.path().string(), scene_path)) continue;

            // Load the scene into a temporary registry
            entt::registry scene_registry;
            SceneSerializer scene_serializer(scene_registry);
            if (!scene_serializer.load(entry.path())) {
                continue;  // Skip files that fail to load
            }

            // Find instances of this prefab in the scene
            bool has_instances = false;
            auto view = scene_registry.view<EntityInfo>();
            for (auto entity : view) {
                const auto& info = view.get<EntityInfo>(entity);
                if (info.is_prefab_instance && paths_equal(info.prefab_path, prefab_path)) {
                    scene_serializer.sync_entity_from_prefab(entity, prefab_registry, prefab_root);
                    has_instances = true;
                }
            }

            // Save the scene if it had instances
            if (has_instances) {
                scene_serializer.save(entry.path());
                ++scenes_updated;
            }
        }
    } catch (const std::exception& e) {
        engine::Logger::instance().error("EditorContext", "Error updating project scenes: %s", e.what());
    }

    if (scenes_updated > 0) {
        engine::Logger::instance().info("EditorContext", "Updated %d scene file(s) with prefab changes: %s",
                                         scenes_updated, prefab_path.c_str());
    }
}

void EditorContext::update_material_tables() {
    if (m_runtime && m_runtime->sim_playback()) {
        m_runtime->sim_playback()->update_material_tables();
    }
}

void EditorContext::init_asset_registry(const std::string& project_path) {
    // Build the Assets folder path
    std::filesystem::path assets_path = std::filesystem::path(project_path) / "Assets";

    if (std::filesystem::exists(assets_path)) {
        // Set up callback to update references when external moves are detected
        m_asset_registry.set_moved_callback([this](const std::string& old_path, const std::string& new_path) {
            update_asset_references(old_path, new_path);
        });

        m_asset_registry.init(assets_path.string());

        // Load cached state and detect any moves that happened while editor was closed
        m_asset_registry.load_cache_and_detect_moves();

        engine::Logger::instance().info("EditorContext", "Asset registry initialized at: %s",
                                         assets_path.string().c_str());
    } else {
        engine::Logger::instance().warning("EditorContext", "Assets folder not found: %s",
                                         assets_path.string().c_str());
    }
}

void EditorContext::rescan_assets_for_external_changes() {
    m_asset_registry.rescan();
}

void EditorContext::shutdown_asset_registry() {
    // Save cache before shutdown so we can detect moves on next startup
    m_asset_registry.save_cache();
    m_asset_registry.shutdown();
}

void EditorContext::update_asset_references(const std::string& old_path, const std::string& new_path) {
    namespace fs = std::filesystem;

    int refs_updated = 0;

    // Update references in the current scene registry
    if (m_registry) {
        int before = refs_updated;
        update_registry_paths(*m_registry, old_path, new_path);
        if (refs_updated > before) {
            m_scene_state.mark_dirty();
        }
    }

    // Update references in all scene and prefab files in the project
    const auto& project_path = m_scene_state.project_path();
    if (project_path.empty()) {
        return;
    }

    const auto& current_scene = m_scene_state.scene_path();
    int files_updated = 0;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(project_path)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            if (ext != ".scene" && ext != ".prefab") continue;

            // Skip the currently open scene - it's updated in memory
            if (paths_equal(entry.path().string(), current_scene)) continue;

            update_file_paths(entry.path(), old_path, new_path);
            ++files_updated;
        }
    } catch (const std::exception& e) {
        engine::Logger::instance().error("EditorContext", "Error updating asset references: %s", e.what());
    }

    engine::Logger::instance().info("EditorContext", "Updated asset references: '%s' -> '%s'",
                                     old_path.c_str(), new_path.c_str());
}

void EditorContext::update_registry_paths(entt::registry& reg, const std::string& old_path, const std::string& new_path) {
    // Update PixelGridComponent paths
    auto pxg_view = reg.view<engine::simulation::PixelGridComponent>();
    for (auto entity : pxg_view) {
        auto& comp = pxg_view.get<engine::simulation::PixelGridComponent>(entity);
        if (paths_equal(comp.pixel_grid_path, old_path)) {
            comp.pixel_grid_path = new_path;
            comp.loaded = false;  // Mark for reload
        }
    }

    // Update Image sprite_path
    auto img_view = reg.view<engine::render::Image>();
    for (auto entity : img_view) {
        auto& comp = img_view.get<engine::render::Image>(entity);
        if (paths_equal(comp.sprite_path, old_path)) {
            comp.sprite_path = new_path;
        }
    }

    // Update Text font_path
    auto text_view = reg.view<engine::render::Text>();
    for (auto entity : text_view) {
        auto& comp = text_view.get<engine::render::Text>(entity);
        if (paths_equal(comp.font_path, old_path)) {
            comp.font_path = new_path;
        }
    }

    // Update EntityInfo prefab_path
    auto info_view = reg.view<EntityInfo>();
    for (auto entity : info_view) {
        auto& info = info_view.get<EntityInfo>(entity);
        if (info.is_prefab_instance && paths_equal(info.prefab_path, old_path)) {
            info.prefab_path = new_path;
        }
    }
}

void EditorContext::update_file_paths(const std::filesystem::path& file_path, const std::string& old_path, const std::string& new_path) {
    try {
        // Read the file
        std::ifstream in_file(file_path);
        if (!in_file.is_open()) return;

        std::string content((std::istreambuf_iterator<char>(in_file)),
                            std::istreambuf_iterator<char>());
        in_file.close();

        // Parse JSON
        nlohmann::json json = nlohmann::json::parse(content, nullptr, false);
        if (json.is_discarded()) return;

        // Track if we made changes
        bool modified = false;

        // Helper to normalize paths for comparison
        auto normalize = [](const std::string& p) {
            return engine::platform::normalize_path_for_comparison(p);
        };

        std::string old_norm = normalize(old_path);

        // Recursively update all string values that match old_path
        std::function<void(nlohmann::json&)> update_paths = [&](nlohmann::json& node) {
            if (node.is_string()) {
                std::string val = node.get<std::string>();
                if (normalize(val) == old_norm) {
                    node = new_path;
                    modified = true;
                }
            } else if (node.is_object()) {
                for (auto& [key, val] : node.items()) {
                    update_paths(val);
                }
            } else if (node.is_array()) {
                for (auto& elem : node) {
                    update_paths(elem);
                }
            }
        };

        update_paths(json);

        // Write back if modified
        if (modified) {
            std::ofstream out_file(file_path);
            if (out_file.is_open()) {
                out_file << json.dump(2);
                engine::Logger::instance().info("EditorContext", "Updated references in: %s",
                                                 file_path.string().c_str());
            }
        }
    } catch (const std::exception& e) {
        engine::Logger::instance().error("EditorContext", "Failed to update file '%s': %s",
                                          file_path.string().c_str(), e.what());
    }
}

}

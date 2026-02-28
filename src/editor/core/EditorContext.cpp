#include "EditorContext.h"
#include "EditorComponents.h"
#include "SimulationPlayback.h"
#include "editor/commands/Command.h"
#include "editor/commands/EntityCommands.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/Logger.h"
#include "engine/core/Transform.h"
#include "engine/platform/PlatformUtils.h"
#include <cmath>
#include <filesystem>

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

}

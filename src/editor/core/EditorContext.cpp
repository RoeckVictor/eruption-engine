#include "EditorContext.h"
#include "EditorComponents.h"
#include "editor/commands/Command.h"
#include "editor/commands/EntityCommands.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/Logger.h"
#include "engine/core/Transform.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace editor {

namespace {

/// Normalize a path for comparison (handles slashes, case, and makes it canonical if possible)
std::string normalize_path_for_comparison(const std::string& path) {
    if (path.empty()) return path;

    namespace fs = std::filesystem;
    try {
        // Use weakly_canonical to normalize without requiring the file to exist
        fs::path normalized = fs::weakly_canonical(fs::path(path));
        std::string result = normalized.string();

        // On Windows, convert to lowercase for case-insensitive comparison
#ifdef _WIN32
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
        return result;
    } catch (const std::exception&) {
        // Fallback: just normalize slashes
        std::string result = path;
        std::replace(result.begin(), result.end(), '\\', '/');
#ifdef _WIN32
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
        return result;
    }
}

/// Compare two paths for equality (handles platform differences)
bool paths_equal(const std::string& a, const std::string& b) {
    return normalize_path_for_comparison(a) == normalize_path_for_comparison(b);
}

} // anonymous namespace

EditorContext::EditorContext() = default;
EditorContext::~EditorContext() = default;

bool EditorContext::is_selected(entt::entity entity) const {
    if (m_editing_override.selection) {
        // Override path: no set mirror, fall back to linear search
        const auto& sel = *m_editing_override.selection;
        return std::find(sel.begin(), sel.end(), entity) != sel.end();
    }
    return m_selection_set.count(entity) > 0;
}

void EditorContext::select(entt::entity entity) {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    sel.clear();
    if (!m_editing_override.selection) m_selection_set.clear();
    if (entity != entt::null) {
        sel.push_back(entity);
        if (!m_editing_override.selection) m_selection_set.insert(entity);
    }
    notify_selection_changed();
}

void EditorContext::add_to_selection(entt::entity entity) {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    if (entity != entt::null && !is_selected(entity)) {
        sel.push_back(entity);
        if (!m_editing_override.selection) m_selection_set.insert(entity);
        notify_selection_changed();
    }
}

void EditorContext::remove_from_selection(entt::entity entity) {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    auto it = std::find(sel.begin(), sel.end(), entity);
    if (it != sel.end()) {
        sel.erase(it);
        if (!m_editing_override.selection) m_selection_set.erase(entity);
        notify_selection_changed();
    }
}

void EditorContext::clear_selection() {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    if (!sel.empty()) {
        sel.clear();
        if (!m_editing_override.selection) m_selection_set.clear();
        notify_selection_changed();
    }
}

void EditorContext::select_multiple(const std::vector<entt::entity>& entities) {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    sel = entities;
    // Remove any null entities
    sel.erase(
        std::remove(sel.begin(), sel.end(), entt::null),
        sel.end()
    );
    if (!m_editing_override.selection) {
        m_selection_set.clear();
        m_selection_set.insert(sel.begin(), sel.end());
    }
    notify_selection_changed();
}

void EditorContext::set_selection_changed_callback(SelectionChangedCallback callback) {
    m_selection_callback = std::move(callback);
}

void EditorContext::set_registry(entt::registry* registry) {
    m_registry = registry;
    // Clear selection when registry changes
    clear_selection();
}

void EditorContext::mark_dirty() {
    if (m_editing_override.mark_dirty) {
        m_editing_override.mark_dirty();
    } else {
        m_dirty = true;
    }
}

void EditorContext::set_editing_override(const EditingOverride& override) {
    m_editing_override = override;
    notify_selection_changed();
}

void EditorContext::clear_editing_override() {
    if (m_editing_override.registry) {
        m_editing_override = {};
        notify_selection_changed();
    }
}

void EditorContext::clear_dirty() {
    m_dirty = false;
}

void EditorContext::set_current_scene_path(const std::string& path) {
    m_scene_path = path;
}

void EditorContext::notify_selection_changed() {
    if (m_selection_callback) {
        m_selection_callback();
    }
}

void EditorContext::execute_command(std::unique_ptr<Command> cmd) {
    if (cmd) {
        m_history.execute(std::move(cmd));
        mark_dirty();
    }
}

void EditorContext::undo() {
    if (m_history.can_undo()) {
        m_history.undo();
        mark_dirty();
    }
}

void EditorContext::redo() {
    if (m_history.can_redo()) {
        m_history.redo();
        mark_dirty();
    }
}

void EditorContext::copy_selection() {
    auto* reg = registry();
    const auto& sel = selection();
    if (!reg || sel.empty()) {
        return;
    }

    SceneSerializer serializer(*reg);
    nlohmann::json json = serializer.serialize_entities(sel);
    m_clipboard = json.dump();

    engine::Logger::instance().info("Editor", "Copied %zu entities to clipboard", sel.size());
}

void EditorContext::paste() {
    auto* reg = registry();
    if (!reg || m_clipboard.empty()) {
        return;
    }

    auto cmd = std::make_unique<PasteEntitiesCommand>(reg, this, m_clipboard);
    execute_command(std::move(cmd));
}

void EditorContext::set_component_clipboard(const std::string& data, std::type_index type) {
    m_component_clipboard = data;
    m_component_clipboard_type = type;
    engine::Logger::instance().info("Editor", "Copied component to clipboard");
}

void EditorContext::duplicate_selection() {
    auto* reg = registry();
    const auto& sel = selection();
    if (!reg || sel.empty()) {
        return;
    }

    copy_selection();
    paste();
}

void EditorContext::focus_on_selection() {
    auto* reg = registry();
    const auto& sel = selection();
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
        m_camera.x = (min_x + max_x) * 0.5f;
        m_camera.y = (min_y + max_y) * 0.5f;

        engine::Logger::instance().info("Editor", "Focused camera on selection at (%.1f, %.1f)",
                                         m_camera.x, m_camera.y);
    }
}

float EditorContext::snap_to_grid(float value) const {
    if (!m_snap_enabled || m_grid_size <= 0.0f) {
        return value;
    }
    return std::round(value / m_grid_size) * m_grid_size;
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

        m_dirty = true;
        engine::Logger::instance().info("EditorContext", "Synced %zu prefab instance(s) in current scene from: %s",
                                         instances.size(), prefab_path.c_str());
    }

    // Also update all other scene files in the project
    if (!m_project_path.empty()) {
        sync_prefab_to_project_scenes(prefab_path, temp_registry, prefab_root);
    }
}

void EditorContext::sync_prefab_to_project_scenes(const std::string& prefab_path,
                                                   entt::registry& prefab_registry,
                                                   entt::entity prefab_root) {
    namespace fs = std::filesystem;

    if (m_project_path.empty()) return;

    int scenes_updated = 0;

    // Find all .scene files in the project
    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_project_path)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".scene") continue;

            // Skip the currently open scene - it's already updated in memory
            if (paths_equal(entry.path().string(), m_scene_path)) continue;

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

}

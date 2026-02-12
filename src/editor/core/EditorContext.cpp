#include "EditorContext.h"
#include "EditorComponents.h"
#include "editor/commands/Command.h"
#include "editor/commands/EntityCommands.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/Logger.h"
#include <algorithm>
#include <cmath>

namespace editor {

EditorContext::EditorContext() = default;
EditorContext::~EditorContext() = default;

bool EditorContext::is_selected(entt::entity entity) const {
    const auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    return std::find(sel.begin(), sel.end(), entity) != sel.end();
}

void EditorContext::select(entt::entity entity) {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    sel.clear();
    if (entity != entt::null) {
        sel.push_back(entity);
    }
    notify_selection_changed();
}

void EditorContext::add_to_selection(entt::entity entity) {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    if (entity != entt::null && !is_selected(entity)) {
        sel.push_back(entity);
        notify_selection_changed();
    }
}

void EditorContext::remove_from_selection(entt::entity entity) {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    auto it = std::find(sel.begin(), sel.end(), entity);
    if (it != sel.end()) {
        sel.erase(it);
        notify_selection_changed();
    }
}

void EditorContext::clear_selection() {
    auto& sel = m_editing_override.selection ? *m_editing_override.selection : m_selection;
    if (!sel.empty()) {
        sel.clear();
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

    try {
        nlohmann::json json = nlohmann::json::parse(m_clipboard);
        SceneSerializer serializer(*reg);
        auto new_entities = serializer.deserialize_entities(json);

        if (!new_entities.empty()) {
            // Offset pasted entities slightly so they don't overlap with originals
            for (auto entity : new_entities) {
                if (reg->all_of<engine::Transform>(entity)) {
                    auto& transform = reg->get<engine::Transform>(entity);
                    transform.x += 20.0f;
                    transform.y += 20.0f;
                }

                // Generate new GUIDs for pasted entities
                if (reg->all_of<EntityInfo>(entity)) {
                    auto& info = reg->get<EntityInfo>(entity);
                    // Simple GUID: timestamp + random
                    info.guid = "pasted_" + std::to_string(reinterpret_cast<uintptr_t>(&entity));
                }
            }

            // Select the newly pasted entities
            select_multiple(new_entities);
            mark_dirty();

            engine::Logger::instance().info("Editor", "Pasted %zu entities", new_entities.size());
        }
    } catch (const std::exception& e) {
        engine::Logger::instance().error("Editor", "Failed to paste: %s", e.what());
    }
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

    // Copy current selection
    copy_selection();

    // Paste immediately
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

} // namespace editor

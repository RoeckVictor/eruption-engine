#include "EntityCommands.h"
#include "editor/core/EditorContext.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/Logger.h"

namespace editor {

void AddEntityCommand::execute() {
    if (!m_registry) return;

    if (m_was_executed && m_entity != entt::null) {
        // Re-executing after undo - recreate with same settings
        m_entity = create_entity(*m_registry, m_name);
        if (m_parent != entt::null && m_registry->valid(m_parent)) {
            set_parent(*m_registry, m_entity, m_parent);
        }
    } else {
        // First execution
        m_entity = create_entity(*m_registry, m_name);
        if (m_parent != entt::null && m_registry->valid(m_parent)) {
            set_parent(*m_registry, m_entity, m_parent);
        }
        m_was_executed = true;
    }

    // Select the new entity
    if (m_context) {
        m_context->select(m_entity);
    }
}

void AddEntityCommand::undo() {
    if (!m_registry || m_entity == entt::null) return;

    // Deselect if selected
    if (m_context && m_context->is_selected(m_entity)) {
        m_context->remove_from_selection(m_entity);
    }

    destroy_entity_recursive(*m_registry, m_entity);
}

void DeleteEntityCommand::execute() {
    if (!m_registry || m_entity == entt::null || !m_registry->valid(m_entity)) return;

    // Check if entity was selected
    if (m_context) {
        m_was_selected = m_context->is_selected(m_entity);
        if (m_was_selected) {
            m_context->remove_from_selection(m_entity);
        }
    }

    // Store entity data for undo
    m_stored_entities.clear();

    // Get the parent before storing
    entt::entity parent = entt::null;
    if (m_registry->all_of<Hierarchy>(m_entity)) {
        parent = m_registry->get<Hierarchy>(m_entity).parent;
    }

    store_entity_recursive(m_entity, parent);

    destroy_entity_recursive(*m_registry, m_entity);
}

void DeleteEntityCommand::undo() {
    if (!m_registry || m_stored_entities.empty()) return;

    // Restore entities in order (parents before children)
    std::unordered_map<entt::entity, entt::entity> old_to_new;

    for (const auto& stored : m_stored_entities) {
        entt::entity new_entity = m_registry->create();

        // Restore EntityInfo (enabled_in_hierarchy will be computed after hierarchy is restored)
        m_registry->emplace<EntityInfo>(new_entity, EntityInfo{stored.name, stored.guid, stored.enabled, true, "", false});

        // Restore engine::Transform (world-space) or engine::ScreenRect (screen-space)
        if (stored.has_transform) {
            m_registry->emplace<engine::Transform>(new_entity, stored.transform);
        }
        if (stored.has_screen_rect) {
            m_registry->emplace<engine::ScreenRect>(new_entity, stored.screen_rect);
        }

        // Restore Hierarchy (parent will be set below)
        m_registry->emplace<Hierarchy>(new_entity);

        old_to_new[stored.entity] = new_entity;
    }

    // Restore parent-child relationships
    for (const auto& stored : m_stored_entities) {
        entt::entity new_entity = old_to_new[stored.entity];

        if (stored.parent != entt::null) {
            auto it = old_to_new.find(stored.parent);
            if (it != old_to_new.end()) {
                set_parent(*m_registry, new_entity, it->second);
            } else if (m_registry->valid(stored.parent)) {
                set_parent(*m_registry, new_entity, stored.parent);
            }
        }
    }

    // Update m_entity to the new root entity
    if (!m_stored_entities.empty()) {
        m_entity = old_to_new[m_stored_entities[0].entity];
    }

    // Re-select if it was selected
    if (m_context && m_was_selected && m_entity != entt::null) {
        m_context->select(m_entity);
    }
}

void DeleteEntityCommand::store_entity_recursive(entt::entity entity, entt::entity parent) {
    if (!m_registry->valid(entity)) return;

    StoredEntity stored;
    stored.entity = entity;
    stored.parent = parent;

    // Store EntityInfo
    if (m_registry->all_of<EntityInfo>(entity)) {
        const auto& info = m_registry->get<EntityInfo>(entity);
        stored.name = info.name;
        stored.guid = info.guid;
        stored.enabled = info.enabled;
    } else {
        stored.name = "Entity";
        stored.guid = "";
        stored.enabled = true;
    }

    // Store engine::Transform (world-space entities)
    if (m_registry->all_of<engine::Transform>(entity)) {
        stored.has_transform = true;
        stored.transform = m_registry->get<engine::Transform>(entity);
    }

    // Store engine::ScreenRect (screen-space entities)
    if (m_registry->all_of<engine::ScreenRect>(entity)) {
        stored.has_screen_rect = true;
        stored.screen_rect = m_registry->get<engine::ScreenRect>(entity);
    }

    // Store children references
    if (m_registry->all_of<Hierarchy>(entity)) {
        stored.children = m_registry->get<Hierarchy>(entity).children;
    }

    m_stored_entities.push_back(stored);

    // Recursively store children
    for (auto child : stored.children) {
        store_entity_recursive(child, entity);
    }
}

void PasteEntitiesCommand::regenerate_guids_recursive(entt::entity entity) {
    if (!m_registry->valid(entity)) return;

    // Generate new GUID for this entity
    if (m_registry->all_of<EntityInfo>(entity)) {
        auto& info = m_registry->get<EntityInfo>(entity);
        info.guid = generate_entity_guid();
    }

    // Recursively process children
    if (m_registry->all_of<Hierarchy>(entity)) {
        const auto& hierarchy = m_registry->get<Hierarchy>(entity);
        for (auto child : hierarchy.children) {
            regenerate_guids_recursive(child);
        }
    }
}

void PasteEntitiesCommand::execute() {
    if (!m_registry || m_clipboard_data.empty()) return;

    try {
        nlohmann::json json = nlohmann::json::parse(m_clipboard_data);
        SceneSerializer serializer(*m_registry);
        m_pasted_entities = serializer.deserialize_entities(json);

        for (auto entity : m_pasted_entities) {
            // Offset pasted entities so they don't overlap originals
            if (m_registry->all_of<engine::Transform>(entity)) {
                auto& transform = m_registry->get<engine::Transform>(entity);
                transform.x += 20.0f;
                transform.y += 20.0f;
            } else if (m_registry->all_of<engine::ScreenRect>(entity)) {
                auto& rect = m_registry->get<engine::ScreenRect>(entity);
                rect.offset_x += 20.0f;
                rect.offset_y += 20.0f;
            }

            // Generate new GUIDs for pasted entities AND all their children
            regenerate_guids_recursive(entity);
        }

        // Save previous selection for undo, then select pasted entities
        if (m_context) {
            m_previous_selection = std::vector<entt::entity>(
                m_context->selection().begin(), m_context->selection().end());
            m_context->select_multiple(m_pasted_entities);
        }

        engine::Logger::instance().info("Editor", "Pasted %zu entities", m_pasted_entities.size());
    } catch (const std::exception& e) {
        engine::Logger::instance().error("Editor", "Failed to paste: %s", e.what());
    }
}

void PasteEntitiesCommand::undo() {
    if (!m_registry) return;

    // Destroy all pasted entities
    for (auto entity : m_pasted_entities) {
        if (m_registry->valid(entity)) {
            destroy_entity_recursive(*m_registry, entity);
        }
    }
    m_pasted_entities.clear();

    // Restore previous selection
    if (m_context) {
        m_context->select_multiple(m_previous_selection);
    }
}

}

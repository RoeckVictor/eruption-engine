#include "EntityCommands.h"
#include "editor/core/EditorContext.h"

namespace editor {

// --- AddEntityCommand ---

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

    // Destroy the entity
    destroy_entity_recursive(*m_registry, m_entity);
}

// --- DeleteEntityCommand ---

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

    // Now delete
    destroy_entity_recursive(*m_registry, m_entity);
}

void DeleteEntityCommand::undo() {
    if (!m_registry || m_stored_entities.empty()) return;

    // Restore entities in order (parents before children)
    std::unordered_map<entt::entity, entt::entity> old_to_new;

    for (const auto& stored : m_stored_entities) {
        entt::entity new_entity = m_registry->create();

        // Restore EntityInfo
        m_registry->emplace<EntityInfo>(new_entity, EntityInfo{stored.name, "", stored.enabled, "", false});

        // Restore Transform
        m_registry->emplace<Transform>(new_entity, stored.transform);

        // Restore Hierarchy (parent will be set below)
        m_registry->emplace<Hierarchy>(new_entity);

        old_to_new[stored.entity] = new_entity;
    }

    // Restore parent-child relationships
    for (const auto& stored : m_stored_entities) {
        entt::entity new_entity = old_to_new[stored.entity];

        // Set parent
        if (stored.parent != entt::null) {
            auto it = old_to_new.find(stored.parent);
            if (it != old_to_new.end()) {
                // Parent was also deleted and restored
                set_parent(*m_registry, new_entity, it->second);
            } else if (m_registry->valid(stored.parent)) {
                // Parent still exists
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
        stored.enabled = info.enabled;
    } else {
        stored.name = "Entity";
        stored.enabled = true;
    }

    // Store Transform
    if (m_registry->all_of<Transform>(entity)) {
        stored.transform = m_registry->get<Transform>(entity);
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

entt::entity DeleteEntityCommand::restore_entity(const StoredEntity& stored) {
    // This method is not used in current implementation
    // Keeping for potential future use
    return entt::null;
}

} // namespace editor

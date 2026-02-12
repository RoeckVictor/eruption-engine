#pragma once

#include "Command.h"
#include "editor/core/EditorComponents.h"
#include <entt/entt.hpp>
#include <string>
#include <optional>

namespace editor {

class EditorContext;

/// Command for adding a new entity.
class AddEntityCommand : public Command {
public:
    AddEntityCommand(entt::registry* registry, EditorContext* context,
                     const std::string& name = "New Entity",
                     entt::entity parent = entt::null)
        : m_registry(registry)
        , m_context(context)
        , m_name(name)
        , m_parent(parent)
    {}

    void execute() override;
    void undo() override;

    std::string name() const override { return "Add Entity"; }

    /// Get the created entity (valid after execute).
    entt::entity created_entity() const { return m_entity; }

private:
    entt::registry* m_registry;
    EditorContext* m_context;
    std::string m_name;
    entt::entity m_parent;
    entt::entity m_entity = entt::null;

    // Stored state for undo
    bool m_was_executed = false;
};

/// Command for deleting an entity and its children.
class DeleteEntityCommand : public Command {
public:
    DeleteEntityCommand(entt::registry* registry, EditorContext* context,
                        entt::entity entity)
        : m_registry(registry)
        , m_context(context)
        , m_entity(entity)
    {}

    void execute() override;
    void undo() override;

    std::string name() const override { return "Delete Entity"; }

private:
    // Stored entity data for restoration
    struct StoredEntity {
        entt::entity entity;
        std::string name;
        bool enabled;
        engine::Transform transform;
        entt::entity parent;
        std::vector<entt::entity> children;
    };

    void store_entity_recursive(entt::entity entity, entt::entity parent);
    entt::entity restore_entity(const StoredEntity& stored);

    entt::registry* m_registry;
    EditorContext* m_context;
    entt::entity m_entity;

    std::vector<StoredEntity> m_stored_entities;
    bool m_was_selected = false;
};

/// Command for renaming an entity.
class RenameEntityCommand : public Command {
public:
    RenameEntityCommand(entt::registry* registry, entt::entity entity,
                        const std::string& old_name, const std::string& new_name)
        : m_registry(registry)
        , m_entity(entity)
        , m_old_name(old_name)
        , m_new_name(new_name)
    {}

    void execute() override {
        if (m_registry && m_registry->valid(m_entity) && m_registry->all_of<EntityInfo>(m_entity)) {
            m_registry->get<EntityInfo>(m_entity).name = m_new_name;
        }
    }

    void undo() override {
        if (m_registry && m_registry->valid(m_entity) && m_registry->all_of<EntityInfo>(m_entity)) {
            m_registry->get<EntityInfo>(m_entity).name = m_old_name;
        }
    }

    std::string name() const override { return "Rename Entity"; }

private:
    entt::registry* m_registry;
    entt::entity m_entity;
    std::string m_old_name;
    std::string m_new_name;
};

/// Command for reparenting an entity.
class ReparentCommand : public Command {
public:
    ReparentCommand(entt::registry* registry, entt::entity entity,
                    entt::entity old_parent, entt::entity new_parent)
        : m_registry(registry)
        , m_entity(entity)
        , m_old_parent(old_parent)
        , m_new_parent(new_parent)
    {}

    void execute() override {
        if (m_registry && m_registry->valid(m_entity)) {
            set_parent(*m_registry, m_entity, m_new_parent);
        }
    }

    void undo() override {
        if (m_registry && m_registry->valid(m_entity)) {
            set_parent(*m_registry, m_entity, m_old_parent);
        }
    }

    std::string name() const override { return "Reparent Entity"; }

private:
    entt::registry* m_registry;
    entt::entity m_entity;
    entt::entity m_old_parent;
    entt::entity m_new_parent;
};

} // namespace editor

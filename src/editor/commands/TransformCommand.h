#pragma once

#include "Command.h"
#include "editor/core/EditorComponents.h"
#include <entt/entt.hpp>

namespace editor {

class TransformCommand : public Command {
public:
    TransformCommand(entt::registry* registry, entt::entity entity,
                     const engine::Transform& old_transform, const engine::Transform& new_transform)
        : m_registry(registry)
        , m_entity(entity)
        , m_old_transform(old_transform)
        , m_new_transform(new_transform)
    {}

    void execute() override {
        if (m_registry && m_registry->valid(m_entity) && m_registry->all_of<engine::Transform>(m_entity)) {
            m_registry->get<engine::Transform>(m_entity) = m_new_transform;
        }
    }

    void undo() override {
        if (m_registry && m_registry->valid(m_entity) && m_registry->all_of<engine::Transform>(m_entity)) {
            m_registry->get<engine::Transform>(m_entity) = m_old_transform;
        }
    }

    std::string name() const override {
        return "engine::Transform Entity";
    }

    bool is_mergeable() const override { return true; }

    bool try_merge(const Command* newer) override {
        if (auto* tc = dynamic_cast<const TransformCommand*>(newer)) {
            // Only merge if same entity and commands are close in time (< 0.5 seconds)
            if (tc->m_entity == m_entity && tc->m_registry == m_registry) {
                double time_diff = tc->timestamp() - timestamp();
                if (time_diff < 0.5) {
                    m_new_transform = tc->m_new_transform;
                    return true;
                }
            }
        }
        return false;
    }

private:
    entt::registry* m_registry;
    entt::entity m_entity;
    engine::Transform m_old_transform;
    engine::Transform m_new_transform;
};

}

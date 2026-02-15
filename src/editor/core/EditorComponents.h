#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include "engine/core/Transform.h"
#include "engine/core/TransformSystem.h"

namespace editor {

/// Component that stores entity metadata for the editor
struct EntityInfo {
    std::string name = "Entity";
    std::string guid;
    bool enabled = true;
    bool enabled_in_hierarchy = true;

    // Prefab information (if this entity came from a prefab)
    std::string prefab_path;
    bool is_prefab_instance = false;

    std::vector<std::string> component_order;
};

using engine::Hierarchy;

void set_parent(entt::registry& registry, entt::entity child, entt::entity new_parent);
void remove_from_parent(entt::registry& registry, entt::entity child);

std::vector<entt::entity> get_root_entities(entt::registry& registry);

void update_world_transforms(entt::registry& registry);

void set_entity_enabled(entt::registry& registry, entt::entity entity, bool enabled);
void update_enabled_in_hierarchy(entt::registry& registry, entt::entity entity);

void init_component_type_registry();

entt::entity create_entity(entt::registry& registry, const std::string& name = "Entity");
void destroy_entity_recursive(entt::registry& registry, entt::entity entity);

}

namespace engine::reflection { class TypeInfo; }

namespace editor {
void apply_component_order(
    entt::registry& registry, entt::entity entity,
    std::vector<const engine::reflection::TypeInfo*>& types);

}

#pragma once

#include <entt/entt.hpp>
#include <vector>

namespace engine {

/// Component for parent-child hierarchy.
/// Stores both parent reference and children vector for O(1) child access.
/// Used by TransformSystem for world transform computation and by the editor
/// for hierarchy display, serialization, and undo/redo.
struct Hierarchy {
    entt::entity parent = entt::null;
    std::vector<entt::entity> children;
};

} // namespace engine

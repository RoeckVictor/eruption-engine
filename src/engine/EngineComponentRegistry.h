#pragma once

namespace engine::prefab {
class ComponentRegistry;
}

namespace engine {

/// Register all engine components with the ComponentRegistry for prefab support.
/// This includes Transform, Camera2D, Animator, and other engine components.
/// Call this before registering game-specific components.
void register_engine_components(engine::prefab::ComponentRegistry& registry);

} // namespace engine

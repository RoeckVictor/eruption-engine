#pragma once

#include "engine/core/System.h"
#include "engine/render/DebugRenderer.h"
#include <entt/fwd.hpp>

namespace engine::render { struct Camera2D; }
namespace engine::physics { class PhysicsWorld; class PixelBodyManager; }

namespace game {

/// Renders debug overlays (collision shapes, etc.) when debug_draw is enabled.
class DebugRenderSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void shutdown() override;
    void render(engine::Engine& engine) override;

private:
    entt::registry* m_registry = nullptr;
    engine::render::Camera2D* m_camera = nullptr;
    engine::physics::PhysicsWorld* m_physics_world = nullptr;
    engine::physics::PixelBodyManager* m_body_manager = nullptr;
    engine::render::DebugRenderer m_debug_renderer;

    void draw_body_collision_shapes();
    void draw_terrain_colliders();
};

} // namespace game

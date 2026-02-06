#pragma once

#include "engine/scene/Scene.h"
#include "engine/render/Camera2D.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/PixelBodyManager.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/particles/ParticleSimulation.h"
#include "engine/particles/ParticleRenderer.h"
#include "engine/prefab/ComponentRegistry.h"
#include "engine/prefab/PrefabManager.h"
#include "game/GameConfig.h"
#include "game/GameContext.h"
#include "game/world/World.h"
#include "game/systems/GameInputSystem.h"
#include "game/systems/ToolSystem.h"
#include "game/systems/PlayerInputSystem.h"
#include "game/systems/CameraSystem.h"
#include "game/systems/PlayerSystem.h"
#include "game/systems/SimulationPipelineSystem.h"
#include "game/systems/RigidBodySyncSystem.h"
#include "game/systems/GridRenderSystem.h"
#include "game/systems/ParticleRenderSystem.h"
#include "game/systems/EntityRenderSystem.h"
#include "game/systems/DebugRenderSystem.h"
#include <optional>

namespace game {

/// Simple flat-floor demo scene for testing engine systems.
/// Configuration loaded from game/game_config.json.
class DemoScene : public engine::scene::Scene {
public:
    const char* name() const override { return "DemoScene"; }
    engine::Result<void, engine::ErrorInfo> on_enter(engine::Engine& engine) override;
    void on_exit(engine::Engine& engine) override;

private:
    GameConfig m_game_config;
    entt::entity m_player_entity{entt::null};

    // Prefab system
    engine::prefab::ComponentRegistry m_component_registry;
    engine::prefab::PrefabManager m_prefab_manager;

    World m_world;
    engine::render::Camera2D m_camera;
    engine::physics::PhysicsWorld m_physics_world;
    engine::physics::PixelBodyManager m_body_manager;
    engine::particles::ParticleBuffer m_particle_buffer;
    engine::particles::ParticleSimulation m_particle_sim;
    engine::particles::ParticleRenderer m_particle_renderer;
    std::optional<GameContext> m_context;

    // Systems (owned here, registered into the scene's SystemManager)
    GameInputSystem m_game_input;
    ToolSystem m_tool_system;
    PlayerInputSystem m_input_system;
    CameraSystem m_camera_system;
    PlayerSystem m_player_system;
    SimulationPipelineSystem m_sim_system;
    RigidBodySyncSystem m_rigidbody_sync_system;
    GridRenderSystem m_render_system;
    ParticleRenderSystem m_particle_render_system;
    EntityRenderSystem m_entity_render_system;
    DebugRenderSystem m_debug_render_system;

    void spawn_player(float x, float y);
    void respawn_player();
    void fill_floor();
};

} // namespace game

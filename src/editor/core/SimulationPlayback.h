#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <vector>

#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MargolusSimulation.h"
#include "engine/physics/TerrainColliderManager.h"
#include "engine/graphics/RenderContext.h"
#include "engine/graphics/Texture.h"
#include "engine/graphics/ShaderStorageBuffer.h"
#include "engine/graphics/Shader.h"

namespace engine::physics { class PhysicsWorld; }

namespace editor {

struct SimSurfaceState {
    engine::simulation::PixelGrid pixel_grid;
    engine::simulation::MargolusSimulation simulation;
    engine::graphics::Texture color_texture;
    engine::graphics::ShaderStorageBuffer palette_ssbo;
    std::unique_ptr<engine::physics::TerrainColliderManager> terrain_colliders;
    entt::entity entity = entt::null;
    int width = 0;
    int height = 0;
};

class SimulationPlayback {
public:
    SimulationPlayback(entt::registry& registry);
    ~SimulationPlayback();

    void init(engine::physics::PhysicsWorld* physics_world);
    void update(uint64_t frame_count);
    void shutdown();

    uint32_t get_sim_texture(entt::entity entity) const;

    const std::vector<std::unique_ptr<SimSurfaceState>>& surfaces() const { return m_surfaces; }

private:
    entt::registry& m_registry;
    std::vector<std::unique_ptr<SimSurfaceState>> m_surfaces;
    engine::graphics::RenderContext m_render_context;
    engine::graphics::Shader m_color_shader;
    engine::physics::PhysicsWorld* m_physics_world = nullptr;
};

}

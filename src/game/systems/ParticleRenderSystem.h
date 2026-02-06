#pragma once

#include "engine/core/System.h"
#include "engine/graphics/Texture.h"

namespace engine::render { struct Camera2D; }
namespace engine::simulation { class PixelGrid; }
namespace engine::particles { class ParticleBuffer; class ParticleRenderer; }

namespace game {

/// Renders free particles on top of the grid using the engine's ParticleRenderer.
class ParticleRenderSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void shutdown() override;
    void render(engine::Engine& engine) override;

private:
    engine::particles::ParticleBuffer* m_buffer = nullptr;
    engine::particles::ParticleRenderer* m_renderer = nullptr;
    engine::render::Camera2D* m_camera = nullptr;
    engine::simulation::PixelGrid* m_grid = nullptr;

    engine::graphics::Texture m_palette;

    void create_palette();
};

} // namespace game

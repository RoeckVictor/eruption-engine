#pragma once

#include "engine/core/System.h"
#include "engine/graphics/Shader.h"
#include "engine/graphics/Texture.h"
#include "engine/render/FullscreenPass.h"

namespace engine::render { struct Camera2D; }
namespace engine::simulation { class PixelGrid; }

namespace game {

class GridRenderSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void shutdown() override;
    void render(engine::Engine& engine) override;

private:
    engine::simulation::PixelGrid* m_grid = nullptr;
    engine::render::Camera2D* m_camera = nullptr;

    engine::graphics::Shader m_grid_shader;
    engine::graphics::Texture m_palette;
    engine::render::FullscreenPass m_fullscreen_pass;

    void create_palette();
};

} // namespace game

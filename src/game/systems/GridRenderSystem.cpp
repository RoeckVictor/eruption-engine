#include "game/systems/GridRenderSystem.h"
#include "engine/core/Engine.h"
#include "engine/render/Camera2D.h"
#include "engine/simulation/PixelGrid.h"
#include "game/GameContext.h"
#include "game/world/World.h"
#include "game/world/MaterialData.h"
#include "game/GameLog.h"

namespace game {

void GridRenderSystem::create_palette() {
    uint8_t palette[256 * 4] = {};

    for (int i = 0; i < MAT_COUNT; i++) {
        uint32_t c = MATERIAL_COLORS[i];
        palette[i * 4 + 0] = (c >> 24) & 0xFF; // R
        palette[i * 4 + 1] = (c >> 16) & 0xFF; // G
        palette[i * 4 + 2] = (c >> 8)  & 0xFF; // B
        palette[i * 4 + 3] = (c >> 0)  & 0xFF; // A
    }

    m_palette.create_1d(256, engine::graphics::TextureFormat::RGBA8,
                        engine::graphics::TextureFilter::Nearest,
                        engine::graphics::TextureWrap::ClampToEdge,
                        palette);
}

bool GridRenderSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_grid = &ctx.world.grid();
    m_camera = &ctx.camera;

    if (!m_grid_shader.load_graphics("shaders/fullscreen_quad.vert",
                                      "shaders/render_grid.frag")) {
        GAME_ERR("Failed to load grid rendering shaders");
        return false;
    }

    if (!m_fullscreen_pass.init()) {
        GAME_ERR("Failed to initialize fullscreen pass");
        return false;
    }

    create_palette();

    // Set constant uniforms once (texture units and grid size never change)
    m_grid_shader.use();
    m_grid_shader.set_int("u_grid", 0);
    m_grid_shader.set_int("u_palette", 1);
    m_grid_shader.set_vec2("u_grid_size", (float)m_grid->width(), (float)m_grid->height());

    GAME_LOG("Grid render system initialized");
    return true;
}

void GridRenderSystem::shutdown() {
    m_grid_shader.destroy();
    m_palette.destroy();
    m_fullscreen_pass.shutdown();
}

void GridRenderSystem::render(engine::Engine& engine) {
    auto& window = engine.window();

    m_grid_shader.use();

    m_grid->current_texture().bind(0);
    m_palette.bind(1);

    // Per-frame uniforms
    m_grid_shader.set_vec2("u_camera_pos", m_camera->x, m_camera->y);
    m_grid_shader.set_vec2("u_screen_size", (float)window.width(), (float)window.height());
    m_grid_shader.set_float("u_zoom", m_camera->zoom);

    m_fullscreen_pass.draw();
}

} // namespace game

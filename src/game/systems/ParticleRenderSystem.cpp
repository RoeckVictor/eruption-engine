#include "game/systems/ParticleRenderSystem.h"
#include "engine/core/Engine.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/particles/ParticleRenderer.h"
#include "engine/render/Camera2D.h"
#include "engine/simulation/PixelGrid.h"
#include "game/GameContext.h"
#include "game/world/World.h"
#include "game/world/MaterialData.h"
#include "game/GameLog.h"

namespace game {

void ParticleRenderSystem::create_palette() {
    uint8_t palette[256 * 4] = {};
    for (int i = 0; i < MAT_COUNT; i++) {
        uint32_t c = MATERIAL_COLORS[i];
        palette[i * 4 + 0] = (c >> 24) & 0xFF;
        palette[i * 4 + 1] = (c >> 16) & 0xFF;
        palette[i * 4 + 2] = (c >> 8)  & 0xFF;
        palette[i * 4 + 3] = (c >> 0)  & 0xFF;
    }
    m_palette.create_1d(256, engine::graphics::TextureFormat::RGBA8,
                        engine::graphics::TextureFilter::Nearest,
                        engine::graphics::TextureWrap::ClampToEdge,
                        palette);
}

bool ParticleRenderSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_buffer = &ctx.particle_buffer;
    m_renderer = &ctx.particle_renderer;
    m_camera = &ctx.camera;
    m_grid = &ctx.world.grid();

    create_palette();
    GAME_LOG("Particle render system initialized");
    return true;
}

void ParticleRenderSystem::shutdown() {
    m_palette.destroy();
}

void ParticleRenderSystem::render(engine::Engine& engine) {
    auto& window = engine.window();
    m_renderer->draw(*m_buffer, m_palette, *m_camera,
                     static_cast<float>(window.width()),
                     static_cast<float>(window.height()));
}

} // namespace game

#include "game/systems/ToolSystem.h"
#include "engine/core/Engine.h"
#include "engine/render/Camera2D.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MaterialDefs.h"
#include "engine/physics/PixelBodyManager.h"
#include "engine/physics/PixelBody.h"
#include "game/GameContext.h"
#include "game/world/World.h"
#include "game/components/Components.h"
#include "game/world/MaterialData.h"
#include "game/asset/PxgSprite.h"
#include "game/GameLog.h"
#include <entt/entt.hpp>
#include <cmath>

namespace game {

using engine::platform::MouseButton;
// CAT_EMPTY already available via MaterialData.h → Materials.h

bool ToolSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    m_grid = &ctx.world.grid();
    m_camera = &ctx.camera;
    m_body_manager = &ctx.body_manager;

    // Try to load paste sprite directly (no prefab needed for raw pixel paste)
    auto paste = load_pxg_sprite("game/object.pxg");
    if (paste) {
        m_paste_sprite = std::move(*paste);
        m_paste_loaded = true;
        GAME_LOG("Loaded paste sprite: object.pxg (%dx%d)", m_paste_sprite.width, m_paste_sprite.height);
    }

    // Load box rigidbody through its prefab (prefab defines sprite path + physics properties)
    auto prefab = load_body_prefab("game/box.prefab");
    if (prefab) {
        m_body_prefab = std::move(*prefab);
        m_body_loaded = true;
    }

    return true;
}

void ToolSystem::erase_body_pixels(float world_x, float world_y, int radius) {
    float r2 = static_cast<float>(radius * radius);

    for (auto& body_ptr : m_body_manager->bodies()) {
        auto& body = *body_ptr;
        if (!b2Body_IsValid(body.body_id())) continue;

        // Skip indestructible bodies (e.g., player)
        if (body.is_indestructible()) continue;
        if (body.pixel_count() == 0) continue;

        float bx = body.world_x(*m_body_manager->physics_world());
        float by = body.world_y(*m_body_manager->physics_world());
        float angle = body.rotation(*m_body_manager->physics_world());
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);
        float cx = body.local_center_x();
        float cy = body.local_center_y();

        int bw = body.width();
        int bh = body.height();

        // Quick AABB check: body's max extent vs erase circle
        float body_half_diag = std::sqrt(static_cast<float>(bw * bw + bh * bh)) * 0.5f;
        float dist = std::sqrt((bx - world_x) * (bx - world_x) +
                               (by - world_y) * (by - world_y));
        if (dist > body_half_diag + static_cast<float>(radius)) continue;

        bool any_destroyed = false;

        for (int ly = 0; ly < bh; ly++) {
            for (int lx = 0; lx < bw; lx++) {
                if (body.get_pixel(lx, ly) == 0) continue;

                // Transform local pixel to world space
                float dx = (static_cast<float>(lx) + 0.5f) - cx;
                float dy = (static_cast<float>(ly) + 0.5f) - cy;
                float wx = bx + dx * cos_a - dy * sin_a;
                float wy = by + dx * sin_a + dy * cos_a;

                float ddx = wx - world_x;
                float ddy = wy - world_y;
                if (ddx * ddx + ddy * ddy <= r2) {
                    body.destroy_pixel(lx, ly);
                    any_destroyed = true;
                }
            }
        }

        if (any_destroyed) {
            body.mark_dirty();
        }
    }
}

void ToolSystem::update(engine::Engine& engine, float /*dt*/) {
    const auto& input = engine.input();
    const auto& window = engine.window();
    const auto& state = m_registry->ctx().get<const GameInputState>();

    // Compute world position for all tool modes
    float world_x, world_y;
    engine::render::screen_to_world(
        *m_camera,
        (float)input.mouse_x(), (float)input.mouse_y(),
        (float)window.width(), (float)window.height(),
        world_x, world_y
    );

    // Left click: action depends on tool mode
    switch (state.tool_mode) {
    case ToolMode::Material:
        if (input.is_mouse_held(MouseButton::Left)) {
            const auto& props = MATERIAL_TABLE[state.selected_material];
            m_grid->spawn_material((int)world_x, (int)world_y, state.brush_radius,
                                  (uint8_t)state.selected_material,
                                  props.category, props.default_temp);
            int r = state.brush_radius;
            m_body_manager->mark_terrain_dirty_region(
                (int)world_x - r, (int)world_y - r, r * 2, r * 2);
        }
        break;

    case ToolMode::PasteSprite:
        if (input.is_mouse_pressed(MouseButton::Left) && m_paste_loaded) {
            int px = (int)world_x - m_paste_sprite.width / 2;
            int py = (int)world_y - m_paste_sprite.height / 2;
            paste_sprite(*m_grid, m_paste_sprite, px, py);
            m_body_manager->mark_terrain_dirty_region(
                px, py, m_paste_sprite.width, m_paste_sprite.height);
        }
        break;

    case ToolMode::SpawnBody:
        if (input.is_mouse_pressed(MouseButton::Left) && m_body_loaded) {
            spawn_sprite_body(*m_body_manager, m_body_prefab.sprite,
                              world_x, world_y, m_body_prefab.dynamic);
        }
        break;
    }

    // Right click: erase (all modes)
    if (input.is_mouse_held(MouseButton::Right)) {
        // Erase grid pixels
        const auto& air_props = MATERIAL_TABLE[MAT_AIR];
        m_grid->spawn_material((int)world_x, (int)world_y, state.brush_radius,
                              MAT_AIR, CAT_EMPTY, air_props.default_temp);

        // Also erase rigidbody pixels in the same radius
        erase_body_pixels(world_x, world_y, state.brush_radius);

        // Mark terrain colliders dirty for the erased region
        int r = state.brush_radius;
        m_body_manager->mark_terrain_dirty_region(
            (int)world_x - r, (int)world_y - r, r * 2, r * 2);
    }
}

} // namespace game

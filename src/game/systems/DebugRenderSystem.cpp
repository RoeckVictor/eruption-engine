#include "game/systems/DebugRenderSystem.h"
#include "engine/core/Engine.h"
#include "engine/render/Camera2D.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/PixelBodyManager.h"
#include "engine/physics/PixelBody.h"
#include "game/GameContext.h"
#include "game/components/Components.h"
#include "game/GameLog.h"
#include <entt/entt.hpp>
#include <box2d/box2d.h>
#include <vector>

namespace game {

bool DebugRenderSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    m_camera = &ctx.camera;
    m_physics_world = &ctx.physics_world;
    m_body_manager = &ctx.body_manager;

    if (!m_debug_renderer.init()) {
        GAME_ERR("Failed to initialize debug renderer");
        return false;
    }
    return true;
}

void DebugRenderSystem::shutdown() {
    m_debug_renderer.shutdown();
}

void DebugRenderSystem::render(engine::Engine& engine) {
    const auto& state = m_registry->ctx().get<const GameInputState>();
    if (!state.debug_draw) return;

    auto& window = engine.window();
    m_debug_renderer.begin(m_camera->x, m_camera->y, m_camera->zoom,
                           window.width(), window.height());

    draw_body_collision_shapes();
    draw_terrain_colliders();

    m_debug_renderer.end();
}

void DebugRenderSystem::draw_body_collision_shapes() {
    float PPM = m_physics_world->pixels_per_meter();

    for (auto& body_ptr : m_body_manager->bodies()) {
        auto& body = *body_ptr;
        b2BodyId bid = body.body_id();
        if (!b2Body_IsValid(bid)) continue;

        b2Transform xf = b2Body_GetTransform(bid);

        // Get all shapes on this body
        int shape_count = b2Body_GetShapeCount(bid);
        if (shape_count <= 0) continue;

        std::vector<b2ShapeId> shapes(shape_count);
        b2Body_GetShapes(bid, shapes.data(), shape_count);

        for (int si = 0; si < shape_count; si++) {
            b2ShapeType type = b2Shape_GetType(shapes[si]);
            if (type != b2_polygonShape) continue;

            b2Polygon poly = b2Shape_GetPolygon(shapes[si]);

            // Draw edges of this polygon
            for (int i = 0; i < poly.count; i++) {
                int j = (i + 1) % poly.count;

                // Transform from body-local meters to world meters
                b2Vec2 wa = b2TransformPoint(xf, poly.vertices[i]);
                b2Vec2 wb = b2TransformPoint(xf, poly.vertices[j]);

                // Convert to pixel space
                float px0 = wa.x * PPM;
                float py0 = wa.y * PPM;
                float px1 = wb.x * PPM;
                float py1 = wb.y * PPM;

                // Green lines for dynamic body collision shapes
                m_debug_renderer.draw_line(px0, py0, px1, py1,
                                           0.0f, 1.0f, 0.3f, 0.8f);
            }
        }
    }
}

void DebugRenderSystem::draw_terrain_colliders() {
    float PPM = m_physics_world->pixels_per_meter();

    // Iterate over hash map - each element is a pair<ChunkCoord, TerrainChunk>
    for (const auto& [coord, chunk] : m_body_manager->terrain_colliders().terrain_chunks()) {
        if (!chunk.active) continue;

        // Iterate all chains in this chunk (multiple disconnected regions)
        for (const auto& chain_id : chunk.chain_ids) {
            if (!b2Chain_IsValid(chain_id)) continue;

            int seg_count = b2Chain_GetSegmentCount(chain_id);
            if (seg_count <= 0) continue;

            std::vector<b2ShapeId> seg_shapes(seg_count);
            b2Chain_GetSegments(chain_id, seg_shapes.data(), seg_count);

            for (int i = 0; i < seg_count; i++) {
                b2ChainSegment cs = b2Shape_GetChainSegment(seg_shapes[i]);

                // Convert from Box2D meters to pixel space
                float px0 = cs.segment.point1.x * PPM;
                float py0 = cs.segment.point1.y * PPM;
                float px1 = cs.segment.point2.x * PPM;
                float py1 = cs.segment.point2.y * PPM;

                // Blue lines for terrain colliders
                m_debug_renderer.draw_line(px0, py0, px1, py1,
                                           0.3f, 0.5f, 1.0f, 0.8f);
            }
        }
    }
}

} // namespace game

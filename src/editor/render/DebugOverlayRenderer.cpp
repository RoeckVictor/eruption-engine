#include "DebugOverlayRenderer.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/EditorPixelGridLoader.h"
#include "editor/core/RuntimeContext.h"
#include "editor/core/SimulationPlayback.h"
#include "engine/core/Logger.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Transform.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Colliders.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/render/Camera2D.h"

#include <cmath>
#include <vector>

namespace editor {

namespace DebugOverlayRenderer {

void render(const DebugOverlayConfig& config) {
    if (!config.registry || !config.visibility || !config.transform || !config.draw_list) {
        engine::Logger::instance().warning("DebugOverlayRenderer",
            "render() called with invalid config (missing required parameters)");
        return;
    }

    render_colliders(config);
    render_terrain_colliders(config);
    render_object_origins(config);
    render_object_names(config);
    render_camera_bounds(config);
    render_rigidbody_velocity(config);
    render_pixel_grid_bounds(config);
    render_parent_child_links(config);
}

void render_colliders(const DebugOverlayConfig& config) {
    if (config.visibility->colliders == GizmoVisibility::None) return;

    auto* registry = config.registry;
    const auto& wts = *config.transform;
    auto* draw_list = config.draw_list;
    const auto& should_draw = config.should_draw;

    // BoxCollider
    {
        auto view = registry->view<engine::Transform, engine::physics::BoxCollider>();
        for (auto entity : view) {
            if (!should_draw(config.visibility->colliders, entity)) continue;
            auto& t = view.get<engine::Transform>(entity);
            auto& box = view.get<engine::physics::BoxCollider>(entity);
            if (!box.material.enabled) continue;

            ImU32 col = box.material.is_trigger ? TRIGGER_COLOR : COLLIDER_COLOR;

            float abs_sx = std::abs(t.world_scale_x);
            float abs_sy = std::abs(t.world_scale_y);
            float hw = box.width * 0.5f * abs_sx;
            float hh = box.height * 0.5f * abs_sy;
            float ox = box.offset_x * t.world_scale_x;
            float oy = box.offset_y * t.world_scale_y;

            float c_rot = box.rotation * engine::DEG_TO_RAD;
            float c_cos = std::cos(c_rot);
            float c_sin = std::sin(c_rot);

            float local_corners[4][2] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };
            float body_local[4][2];
            for (int i = 0; i < 4; i++) {
                body_local[i][0] = local_corners[i][0] * c_cos - local_corners[i][1] * c_sin + ox;
                body_local[i][1] = local_corners[i][0] * c_sin + local_corners[i][1] * c_cos + oy;
            }

            float e_rot = t.world_rotation * engine::DEG_TO_RAD;
            float e_cos = std::cos(e_rot);
            float e_sin = std::sin(e_rot);

            ImVec2 pts[4];
            for (int i = 0; i < 4; i++) {
                float wx = t.world_x + body_local[i][0] * e_cos - body_local[i][1] * e_sin;
                float wy = t.world_y + body_local[i][0] * e_sin + body_local[i][1] * e_cos;
                pts[i] = wts(wx, wy);
            }
            draw_list->AddQuad(pts[0], pts[1], pts[2], pts[3], col, 1.5f);
        }
    }

    // CircleCollider
    {
        auto view = registry->view<engine::Transform, engine::physics::CircleCollider>();
        for (auto entity : view) {
            if (!should_draw(config.visibility->colliders, entity)) continue;
            auto& t = view.get<engine::Transform>(entity);
            auto& circle = view.get<engine::physics::CircleCollider>(entity);
            if (!circle.material.enabled) continue;

            ImU32 col = circle.material.is_trigger ? TRIGGER_COLOR : COLLIDER_COLOR;
            float avg_scale = (std::abs(t.world_scale_x) + std::abs(t.world_scale_y)) * 0.5f;

            float ox = circle.offset_x * t.world_scale_x;
            float oy = circle.offset_y * t.world_scale_y;

            float e_rot = t.world_rotation * engine::DEG_TO_RAD;
            float e_cos = std::cos(e_rot);
            float e_sin = std::sin(e_rot);
            float world_cx = t.world_x + ox * e_cos - oy * e_sin;
            float world_cy = t.world_y + ox * e_sin + oy * e_cos;

            ImVec2 center = wts(world_cx, world_cy);
            float screen_radius = circle.radius * avg_scale * wts.zoom;
            draw_list->AddCircle(center, screen_radius, col, 32, 1.5f);
        }
    }

    // CapsuleCollider
    {
        auto view = registry->view<engine::Transform, engine::physics::CapsuleCollider>();
        for (auto entity : view) {
            if (!should_draw(config.visibility->colliders, entity)) continue;
            auto& t = view.get<engine::Transform>(entity);
            auto& cap = view.get<engine::physics::CapsuleCollider>(entity);
            if (!cap.material.enabled) continue;

            ImU32 col = cap.material.is_trigger ? TRIGGER_COLOR : COLLIDER_COLOR;
            float avg_scale = (std::abs(t.world_scale_x) + std::abs(t.world_scale_y)) * 0.5f;
            float half_len = cap.length * 0.5f * avg_scale;
            float rad = cap.radius * avg_scale;

            float ox = cap.offset_x * t.world_scale_x;
            float oy = cap.offset_y * t.world_scale_y;

            float c_rot = cap.rotation * engine::DEG_TO_RAD;
            float c_cos = std::cos(c_rot);
            float c_sin = std::sin(c_rot);
            float local_ax = -c_sin;
            float local_ay = c_cos;

            float local_top_x = ox + local_ax * half_len;
            float local_top_y = oy + local_ay * half_len;
            float local_bot_x = ox - local_ax * half_len;
            float local_bot_y = oy - local_ay * half_len;

            float e_rot = t.world_rotation * engine::DEG_TO_RAD;
            float e_cos = std::cos(e_rot);
            float e_sin = std::sin(e_rot);

            float top_wx = t.world_x + local_top_x * e_cos - local_top_y * e_sin;
            float top_wy = t.world_y + local_top_x * e_sin + local_top_y * e_cos;
            float bot_wx = t.world_x + local_bot_x * e_cos - local_bot_y * e_sin;
            float bot_wy = t.world_y + local_bot_x * e_sin + local_bot_y * e_cos;

            ImVec2 top_screen = wts(top_wx, top_wy);
            ImVec2 bot_screen = wts(bot_wx, bot_wy);
            float screen_rad = rad * wts.zoom;

            draw_list->AddCircle(top_screen, screen_rad, col, 32, 1.5f);
            draw_list->AddCircle(bot_screen, screen_rad, col, 32, 1.5f);

            float total_rot = (t.world_rotation + cap.rotation) * engine::DEG_TO_RAD;
            float perp_x = std::cos(total_rot);
            float perp_y = std::sin(total_rot);

            float side_wx1 = top_wx + perp_x * rad;
            float side_wy1 = top_wy + perp_y * rad;
            float side_wx2 = bot_wx + perp_x * rad;
            float side_wy2 = bot_wy + perp_y * rad;
            draw_list->AddLine(wts(side_wx1, side_wy1), wts(side_wx2, side_wy2), col, 1.5f);
            side_wx1 = top_wx - perp_x * rad;
            side_wy1 = top_wy - perp_y * rad;
            side_wx2 = bot_wx - perp_x * rad;
            side_wy2 = bot_wy - perp_y * rad;
            draw_list->AddLine(wts(side_wx1, side_wy1), wts(side_wx2, side_wy2), col, 1.5f);
        }
    }

    // DynamicCollider
    if (config.pixel_grid_loader) {
        auto view = registry->view<engine::Transform, engine::physics::DynamicCollider>();
        for (auto entity : view) {
            if (!should_draw(config.visibility->colliders, entity)) continue;
            auto& t = view.get<engine::Transform>(entity);
            auto& dc = view.get<engine::physics::DynamicCollider>(entity);
            if (!dc.material.enabled) continue;

            const DebugContours* debug = config.pixel_grid_loader->get_debug_contours(entity);
            if (!debug) {
                config.pixel_grid_loader->regenerate_debug_contours(entity, dc.simplification);
                debug = config.pixel_grid_loader->get_debug_contours(entity);
            }
            if (!debug || debug->contours.empty()) continue;

            ImU32 col = dc.material.is_trigger ? TRIGGER_COLOR : COLLIDER_COLOR;

            float abs_sx = std::abs(t.world_scale_x);
            float abs_sy = std::abs(t.world_scale_y);
            float e_rot = t.world_rotation * engine::DEG_TO_RAD;
            float e_cos = std::cos(e_rot);
            float e_sin = std::sin(e_rot);

            float grid_height = static_cast<float>(debug->height);
            float origin_x = static_cast<float>(debug->origin_x);
            float origin_y = static_cast<float>(debug->origin_y);

            for (const auto& contour : debug->contours) {
                if (contour.vertices.size() < 3) continue;

                std::vector<ImVec2> screen_pts;
                screen_pts.reserve(contour.vertices.size());

                for (const auto& v : contour.vertices) {
                    float lx = (v.x - origin_x) * abs_sx + dc.offset_x * t.world_scale_x;
                    float ly = (grid_height - v.y - origin_y) * abs_sy + dc.offset_y * t.world_scale_y;
                    float wx = t.world_x + lx * e_cos - ly * e_sin;
                    float wy = t.world_y + lx * e_sin + ly * e_cos;
                    screen_pts.push_back(wts(wx, wy));
                }

                draw_list->AddPolyline(screen_pts.data(),
                                       static_cast<int>(screen_pts.size()),
                                       col, ImDrawFlags_Closed, 1.5f);
            }
        }
    }
}

void render_terrain_colliders(const DebugOverlayConfig& config) {
    if (config.visibility->terrain_colliders == GizmoVisibility::None) return;
    if (!config.is_playing || !config.runtime) return;

    const auto& wts = *config.transform;
    auto* draw_list = config.draw_list;
    const auto& should_draw = config.should_draw;

    for (auto& state : config.runtime->sim_surfaces()) {
        if (!state->terrain_colliders) continue;
        if (!should_draw(config.visibility->terrain_colliders, state->entity)) continue;

        for (auto& [coord, chunk] : state->terrain_colliders->terrain_chunks()) {
            if (!chunk.active) continue;
            for (auto& verts : chunk.debug_verts) {
                if (verts.size() < 3) continue;
                std::vector<ImVec2> screen_pts(verts.size());
                for (size_t i = 0; i < verts.size(); i++) {
                    screen_pts[i] = wts(verts[i].x, verts[i].y);
                }
                draw_list->AddPolyline(screen_pts.data(),
                                       static_cast<int>(screen_pts.size()),
                                       TERRAIN_COLOR, ImDrawFlags_Closed, 1.0f);
            }
        }
    }
}

void render_object_origins(const DebugOverlayConfig& config) {
    if (config.visibility->object_origin == GizmoVisibility::None) return;

    auto* registry = config.registry;
    const auto& wts = *config.transform;
    auto* draw_list = config.draw_list;
    const auto& should_draw = config.should_draw;

    auto view = registry->view<engine::Transform>();
    for (auto entity : view) {
        if (!should_draw(config.visibility->object_origin, entity)) continue;
        auto& t = view.get<engine::Transform>(entity);
        ImVec2 center = wts(t.world_x, t.world_y);
        constexpr float cross = 6.0f;
        draw_list->AddLine(ImVec2(center.x - cross, center.y),
                           ImVec2(center.x + cross, center.y), ORIGIN_COLOR, 1.0f);
        draw_list->AddLine(ImVec2(center.x, center.y - cross),
                           ImVec2(center.x, center.y + cross), ORIGIN_COLOR, 1.0f);
    }
}

void render_object_names(const DebugOverlayConfig& config) {
    if (config.visibility->object_name == GizmoVisibility::None) return;

    auto* registry = config.registry;
    const auto& wts = *config.transform;
    auto* draw_list = config.draw_list;
    const auto& should_draw = config.should_draw;

    auto view = registry->view<engine::Transform, EntityInfo>();
    for (auto entity : view) {
        if (!should_draw(config.visibility->object_name, entity)) continue;
        auto& t = view.get<engine::Transform>(entity);
        auto& info = view.get<EntityInfo>(entity);
        ImVec2 screen_pos = wts(t.world_x, t.world_y);
        ImVec2 text_size = ImGui::CalcTextSize(info.name.c_str());
        draw_list->AddText(ImVec2(screen_pos.x - text_size.x * 0.5f, screen_pos.y - 20.0f),
                           NAME_COLOR, info.name.c_str());
    }
}

void render_camera_bounds(const DebugOverlayConfig& config) {
    if (config.visibility->camera_bounds == GizmoVisibility::None) return;

    auto* registry = config.registry;
    const auto& wts = *config.transform;
    auto* draw_list = config.draw_list;
    const auto& should_draw = config.should_draw;

    auto view = registry->view<engine::Transform, engine::render::Camera2D>();
    for (auto entity : view) {
        if (!should_draw(config.visibility->camera_bounds, entity)) continue;
        auto& t = view.get<engine::Transform>(entity);
        auto& cam = view.get<engine::render::Camera2D>(entity);
        if (!cam.enabled) continue;

        float ref_w = wts.viewport_width;
        float ref_h = wts.viewport_height;
        float half_w = cam.visible_width(ref_w) * 0.5f;
        float half_h = cam.visible_height(ref_h) * 0.5f;

        ImVec2 tl = wts(t.world_x - half_w, t.world_y + half_h);
        ImVec2 br = wts(t.world_x + half_w, t.world_y - half_h);
        draw_list->AddRect(tl, br, CAMERA_COLOR, 0.0f, 0, 1.5f);

        constexpr float bracket_len = 16.0f;
        draw_list->AddLine(tl, ImVec2(tl.x + bracket_len, tl.y), CAMERA_COLOR, 2.0f);
        draw_list->AddLine(tl, ImVec2(tl.x, tl.y + bracket_len), CAMERA_COLOR, 2.0f);
        draw_list->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x - bracket_len, tl.y), CAMERA_COLOR, 2.0f);
        draw_list->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y + bracket_len), CAMERA_COLOR, 2.0f);
        draw_list->AddLine(br, ImVec2(br.x - bracket_len, br.y), CAMERA_COLOR, 2.0f);
        draw_list->AddLine(br, ImVec2(br.x, br.y - bracket_len), CAMERA_COLOR, 2.0f);
        draw_list->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x + bracket_len, br.y), CAMERA_COLOR, 2.0f);
        draw_list->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x, br.y - bracket_len), CAMERA_COLOR, 2.0f);

        ImVec2 center = wts(t.world_x, t.world_y);
        constexpr float icon_size = 8.0f;
        ImVec2 diamond[4] = {
            ImVec2(center.x, center.y - icon_size),
            ImVec2(center.x + icon_size, center.y),
            ImVec2(center.x, center.y + icon_size),
            ImVec2(center.x - icon_size, center.y)
        };
        draw_list->AddQuadFilled(diamond[0], diamond[1], diamond[2], diamond[3],
                                 IM_COL32(220, 50, 220, 100));
        draw_list->AddQuad(diamond[0], diamond[1], diamond[2], diamond[3], CAMERA_COLOR, 1.5f);
        draw_list->AddText(ImVec2(center.x + icon_size + 4, center.y - 7), CAMERA_COLOR, "Camera");
    }
}

void render_rigidbody_velocity(const DebugOverlayConfig& config) {
    if (config.visibility->rigidbody_velocity == GizmoVisibility::None) return;
    if (!config.is_playing || !config.runtime) return;

    auto* registry = config.registry;
    const auto& wts = *config.transform;
    auto* draw_list = config.draw_list;
    const auto& should_draw = config.should_draw;

    auto* physics = config.runtime->physics_world();
    if (!physics) return;

    auto view = registry->view<engine::Transform, engine::physics::Rigidbody>();
    for (auto entity : view) {
        if (!should_draw(config.visibility->rigidbody_velocity, entity)) continue;
        auto& t = view.get<engine::Transform>(entity);
        auto& rb = view.get<engine::physics::Rigidbody>(entity);
        if (!rb.enabled || !b2Body_IsValid(rb.body_id)) continue;

        b2Vec2 vel = physics->get_body_linear_velocity(rb.body_id);
        float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (speed < 0.5f) continue;

        float arrow_scale = 0.1f;
        float arrow_len = std::min(speed * arrow_scale, 100.0f);
        float nx = vel.x / speed;
        float ny = vel.y / speed;

        ImVec2 origin_s = wts(t.world_x, t.world_y);
        ImVec2 tip_s = ImVec2(origin_s.x + nx * arrow_len * wts.zoom,
                              origin_s.y - ny * arrow_len * wts.zoom);

        draw_list->AddLine(origin_s, tip_s, VELOCITY_COLOR, 2.0f);

        float head_size = 6.0f;
        float dir_x = tip_s.x - origin_s.x;
        float dir_y = tip_s.y - origin_s.y;
        float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
        if (dir_len > 1.0f) {
            dir_x /= dir_len;
            dir_y /= dir_len;
            float perp_x = -dir_y;
            float perp_y = dir_x;
            ImVec2 h1(tip_s.x - dir_x * head_size + perp_x * head_size * 0.5f,
                      tip_s.y - dir_y * head_size + perp_y * head_size * 0.5f);
            ImVec2 h2(tip_s.x - dir_x * head_size - perp_x * head_size * 0.5f,
                      tip_s.y - dir_y * head_size - perp_y * head_size * 0.5f);
            draw_list->AddTriangleFilled(tip_s, h1, h2, VELOCITY_COLOR);
        }
    }
}

void render_pixel_grid_bounds(const DebugOverlayConfig& config) {
    if (config.visibility->pixel_grid_bounds == GizmoVisibility::None) return;

    auto* registry = config.registry;
    const auto& wts = *config.transform;
    auto* draw_list = config.draw_list;
    const auto& should_draw = config.should_draw;

    auto view = registry->view<engine::Transform, engine::simulation::PixelGridComponent>();
    for (auto entity : view) {
        if (!should_draw(config.visibility->pixel_grid_bounds, entity)) continue;
        auto& t = view.get<engine::Transform>(entity);
        auto& grid_comp = view.get<engine::simulation::PixelGridComponent>(entity);
        if (grid_comp.width <= 0 || grid_comp.height <= 0) continue;

        float w = static_cast<float>(grid_comp.width);
        float h = static_cast<float>(grid_comp.height);
        float ox = static_cast<float>(grid_comp.origin_x);
        float oy = static_cast<float>(grid_comp.origin_y);
        float sx = t.world_scale_x;
        float sy = t.world_scale_y;
        float rot_rad = t.world_rotation * engine::DEG_TO_RAD;
        float cos_r = std::cos(rot_rad);
        float sin_r = std::sin(rot_rad);

        float lx[4] = {-ox * sx,      (w - ox) * sx, (w - ox) * sx, -ox * sx};
        float ly[4] = {(h - oy) * sy, (h - oy) * sy, -oy * sy,     -oy * sy};

        ImVec2 pts[4];
        for (int i = 0; i < 4; i++) {
            float wx = t.world_x + lx[i] * cos_r - ly[i] * sin_r;
            float wy = t.world_y + lx[i] * sin_r + ly[i] * cos_r;
            pts[i] = wts(wx, wy);
        }
        draw_list->AddQuad(pts[0], pts[1], pts[2], pts[3], GRID_BOUNDS_COLOR, 1.5f);
    }
}

void render_parent_child_links(const DebugOverlayConfig& config) {
    if (config.visibility->parent_child_links == GizmoVisibility::None) return;

    auto* registry = config.registry;
    const auto& wts = *config.transform;
    auto* draw_list = config.draw_list;
    const auto& should_draw = config.should_draw;

    auto view = registry->view<engine::Transform, Hierarchy>();
    for (auto entity : view) {
        auto& hierarchy = view.get<Hierarchy>(entity);
        if (hierarchy.parent == entt::null) continue;
        if (!should_draw(config.visibility->parent_child_links, entity)) continue;
        if (!registry->valid(hierarchy.parent) ||
            !registry->all_of<engine::Transform>(hierarchy.parent)) continue;

        auto& child_t = view.get<engine::Transform>(entity);
        auto& parent_t = registry->get<engine::Transform>(hierarchy.parent);
        ImVec2 child_s = wts(child_t.world_x, child_t.world_y);
        ImVec2 parent_s = wts(parent_t.world_x, parent_t.world_y);
        draw_list->AddLine(parent_s, child_s, LINK_COLOR, 1.0f);
    }
}

} // namespace DebugOverlayRenderer

} // namespace editor

#include "ViewportPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/EditorPixelGridLoader.h"
#include "editor/core/RuntimeContext.h"
#include "editor/core/SimulationPlayback.h"
#include "editor/render/SceneRenderUtils.h"
#include "editor/render/EntityHitDetector.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Transform.h"
#include "engine/core/ScreenRect.h"
#include "engine/core/Logger.h"
#include "engine/core/Engine.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/render/Camera2D.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"

#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>

namespace editor {

ViewportPanel::ViewportPanel(EditorContext& context)
    : Panel("Viewport")
    , m_context(context)
    , m_gizmo_renderer(context)
    , m_scene_renderer(context)
{
}

ViewportPanel::~ViewportPanel() {
    destroy_framebuffer();
}

void ViewportPanel::on_open() {
    // Framebuffer will be created when we know the size
}

void ViewportPanel::on_close() {
    destroy_framebuffer();
}

void ViewportPanel::on_gui() {
    // When the viewport is focused, clear any editing override (e.g., prefab editor)
    // so Inspector and Hierarchy show the main scene context
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        m_context.clear_editing_override();
    }

    // Get available size
    ImVec2 size = ImGui::GetContentRegionAvail();
    int width = static_cast<int>(size.x);
    int height = static_cast<int>(size.y);

    if (width <= 0 || height <= 0) {
        return;
    }

    // Debounced framebuffer resize
    if (m_resize_debouncer.should_resize(m_viewport_width, m_viewport_height, width, height, ImGui::GetIO().DeltaTime)) {
        destroy_framebuffer();
        create_framebuffer(m_resize_debouncer.target_width(), m_resize_debouncer.target_height());
    }

    // Render scene to framebuffer
    render_scene();

    // Display the framebuffer texture
    ImTextureID texture_id = m_framebuffer
        ? (ImTextureID)(uintptr_t)(m_framebuffer->color_attachment(0)->native_handle())
        : 0;

    // UV coordinates depend on texture origin convention
    auto* device = engine::rhi::get_current_device();
    bool flip_y = device && !device->uv_origin_top_left();
    ImVec2 uv0 = flip_y ? ImVec2(0, 1) : ImVec2(0, 0);
    ImVec2 uv1 = flip_y ? ImVec2(1, 0) : ImVec2(1, 1);
    ImGui::Image(texture_id, size, uv0, uv1);

    // Get viewport rect for gizmos and overlay
    ImVec2 viewport_pos = ImGui::GetItemRectMin();
    ImVec2 viewport_size = ImGui::GetItemRectSize();

    // Update gizmo state BEFORE handling input
    // This ensures gizmo hover/drag takes priority over click-to-select
    m_gizmo_renderer.update(viewport_pos, viewport_size);

    // Handle input when hovered
    if (ImGui::IsItemHovered()) {
        handle_input();
    }

    // Render overlay and gizmos
    render_overlay();

    // Render gizmos on top
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    m_gizmo_renderer.render(draw_list, viewport_pos, viewport_size);

}

void ViewportPanel::create_framebuffer(int width, int height) {
    m_viewport_width = width;
    m_viewport_height = height;

    auto* device = engine::rhi::get_current_device();
    if (!device) {
        engine::Logger::instance().error("Viewport", "No RHI device available");
        m_resize_debouncer.set_failed();
        return;
    }

    // Create framebuffer with color and depth attachments using convenience method
    m_framebuffer = device->create_simple_framebuffer(width, height,
                                                       engine::rhi::TextureFormat::RGBA8,
                                                       true);
    if (!m_framebuffer || !m_framebuffer->valid()) {
        engine::Logger::instance().error("Viewport", "Failed to create framebuffer");
        m_framebuffer.reset();
        m_resize_debouncer.set_failed();
    }
}

void ViewportPanel::destroy_framebuffer() {
    m_framebuffer.reset();
    m_viewport_width = 0;
    m_viewport_height = 0;
}

void ViewportPanel::render_scene() {
    if (!m_framebuffer) {
        return;
    }

    auto* ctx = engine::rhi::get_current_context();
    if (!ctx) return;

    ctx->bind_framebuffer(m_framebuffer.get());
    ctx->set_viewport(0, 0, m_viewport_width, m_viewport_height);

    // Clear with a dark color
    ctx->clear(0.15f, 0.15f, 0.18f, 1.0f);
    ctx->clear_depth(1.0f);

    // Render grid
    render_grid();

    // Render scene entities
    render_entities();

    // Render particles from rigidbody-simulation collisions
    // Use the editor viewport camera (same as grid/entity rendering) for consistency
    auto* runtime = m_context.runtime();
    if (runtime && runtime->state() == PlayState::Playing && runtime->sim_playback()) {
        auto& vp_camera = m_context.viewport().camera;
        engine::render::Camera2D particle_camera;
        particle_camera.x = vp_camera.x;
        particle_camera.y = vp_camera.y;
        particle_camera.zoom = vp_camera.zoom;
        runtime->sim_playback()->render_particles(
            particle_camera,
            static_cast<float>(m_viewport_width),
            static_cast<float>(m_viewport_height));
    }

    ctx->bind_framebuffer(nullptr);
}

void ViewportPanel::render_grid() {
    // Grid will be drawn in the overlay using ImGui draw list
    // since we need screen-space coordinates
}

void ViewportPanel::render_entities() {
    // Entity rendering moved to render_overlay() where we can use ImGui draw list
    // This keeps all viewport rendering in one place
}

void ViewportPanel::render_overlay() {
    // Get viewport position
    ImVec2 pos = ImGui::GetItemRectMin();
    ImVec2 size = ImGui::GetItemRectSize();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto& camera = m_context.viewport().camera;

    // Create coordinate transform once for all rendering
    CoordinateTransform wts(pos, size, camera.x, camera.y, camera.zoom);

    // Draw grid if enabled
    if (m_context.viewport().grid_visible) {
        float grid_size = m_context.viewport().grid_size;

        // Calculate visible world-space bounds
        float half_width = (size.x * 0.5f) / camera.zoom;
        float half_height = (size.y * 0.5f) / camera.zoom;

        float world_left = camera.x - half_width;
        float world_right = camera.x + half_width;
        float world_top = camera.y + half_height;
        float world_bottom = camera.y - half_height;

        // Snap to grid boundaries
        float start_x = std::floor(world_left / grid_size) * grid_size;
        float start_y = std::floor(world_bottom / grid_size) * grid_size;

        // Grid line colors
        ImU32 grid_color = IM_COL32(60, 60, 70, 100);
        ImU32 major_grid_color = IM_COL32(80, 80, 90, 150);

        // Draw vertical lines
        for (float x = start_x; x <= world_right; x += grid_size) {
            ImVec2 top = wts(x, world_top);
            ImVec2 bottom = wts(x, world_bottom);

            // Major lines every 5 grid units
            bool is_major = (static_cast<int>(std::round(x / grid_size)) % 5) == 0;
            ImU32 color = is_major ? major_grid_color : grid_color;

            draw_list->AddLine(top, bottom, color);
        }

        // Draw horizontal lines
        for (float y = start_y; y <= world_top; y += grid_size) {
            ImVec2 left = wts(world_left, y);
            ImVec2 right = wts(world_right, y);

            // Major lines every 5 grid units
            bool is_major = (static_cast<int>(std::round(y / grid_size)) % 5) == 0;
            ImU32 color = is_major ? major_grid_color : grid_color;

            draw_list->AddLine(left, right, color);
        }

        // Draw origin axes (thicker, colored)
        // X axis (red) - only if origin is in view
        if (world_bottom <= 0 && world_top >= 0) {
            ImVec2 x_start = wts(world_left, 0);
            ImVec2 x_end = wts(world_right, 0);
            draw_list->AddLine(x_start, x_end, IM_COL32(180, 80, 80, 200), 1.5f);
        }

        // Y axis (green)
        if (world_left <= 0 && world_right >= 0) {
            ImVec2 y_start = wts(0, world_bottom);
            ImVec2 y_end = wts(0, world_top);
            draw_list->AddLine(y_start, y_end, IM_COL32(80, 180, 80, 200), 1.5f);
        }
    }

    // Draw world-space entities (PixelGrid, Image, Text) sorted by layer
    auto* registry = m_context.registry();
    if (registry) {
        // Render all world-space entities using the scene renderer
        m_scene_renderer.render_world_entities(draw_list, *registry, wts, true, m_context.runtime());
        m_scene_renderer.cleanup(m_context.registry());
    }

    // Draw debug overlays (colliders, origins, names, etc.)
    render_debug_overlays(draw_list, wts);

    // Draw camera info in corner
    char info[128];
    snprintf(info, sizeof(info), "Camera: (%.1f, %.1f) Zoom: %.2f", camera.x, camera.y, camera.zoom);
    draw_list->AddText(
        ImVec2(pos.x + 10, pos.y + 10),
        IM_COL32(200, 200, 200, 200),
        info
    );

    // Draw center crosshair (only if grid is not visible - grid has origin axes)
    if (!m_context.viewport().grid_visible) {
        float center_x = pos.x + size.x * 0.5f;
        float center_y = pos.y + size.y * 0.5f;
        float cross_size = 10.0f;
        ImU32 cross_color = IM_COL32(100, 100, 100, 150);

        draw_list->AddLine(
            ImVec2(center_x - cross_size, center_y),
            ImVec2(center_x + cross_size, center_y),
            cross_color
        );
        draw_list->AddLine(
            ImVec2(center_x, center_y - cross_size),
            ImVec2(center_x, center_y + cross_size),
            cross_color
        );
    }

    // Draw dirty indicator
    if (m_context.scene_state().is_dirty()) {
        draw_list->AddText(
            ImVec2(pos.x + size.x - 80, pos.y + 10),
            IM_COL32(255, 200, 100, 255),
            "* Unsaved"
        );
    }

    // Draw selection count
    if (!m_context.selection().selection().empty()) {
        char sel_info[64];
        snprintf(sel_info, sizeof(sel_info), "Selected: %zu", m_context.selection().selection().size());
        draw_list->AddText(
            ImVec2(pos.x + 10, pos.y + 30),
            IM_COL32(150, 200, 255, 200),
            sel_info
        );
    }

    // Draw play mode indicator banner
    if (m_context.is_playing()) {
        ImU32 banner_color;
        const char* banner_text;

        if (m_context.is_paused()) {
            banner_color = IM_COL32(200, 150, 0, 200);
            banner_text = "PAUSED";
        } else {
            banner_color = IM_COL32(50, 180, 50, 200);
            banner_text = "PLAYING";
        }

        // Draw banner at top center
        ImVec2 text_size = ImGui::CalcTextSize(banner_text);
        float banner_width = text_size.x + 40;
        float banner_height = text_size.y + 10;
        float banner_x = pos.x + (size.x - banner_width) * 0.5f;
        float banner_y = pos.y + 5;

        draw_list->AddRectFilled(
            ImVec2(banner_x, banner_y),
            ImVec2(banner_x + banner_width, banner_y + banner_height),
            banner_color,
            4.0f
        );

        draw_list->AddText(
            ImVec2(banner_x + 20, banner_y + 5),
            IM_COL32(255, 255, 255, 255),
            banner_text
        );

        // Draw subtle border around viewport when playing
        draw_list->AddRect(
            pos,
            ImVec2(pos.x + size.x, pos.y + size.y),
            m_context.is_paused() ? IM_COL32(200, 150, 0, 150) : IM_COL32(50, 180, 50, 150),
            0.0f,
            0,
            3.0f
        );
    }
}

void ViewportPanel::render_debug_overlays(ImDrawList* draw_list, const CoordinateTransform& wts) {
    auto* registry = m_context.registry();
    if (!registry) return;

    const auto& vis = m_context.gizmo_visibility();

    auto should_draw = [&](GizmoVisibility v, entt::entity e) -> bool {
        if (v == GizmoVisibility::None) return false;
        if (v == GizmoVisibility::All) return true;
        return m_context.selection().is_selected(e);
    };

    constexpr ImU32 collider_color   = IM_COL32(0, 200, 0, 180);
    constexpr ImU32 trigger_color    = IM_COL32(200, 200, 0, 180);
    constexpr ImU32 origin_color     = IM_COL32(255, 255, 255, 200);
    constexpr ImU32 name_color       = IM_COL32(220, 220, 220, 200);
    constexpr ImU32 camera_color     = IM_COL32(220, 50, 220, 180);
    constexpr ImU32 velocity_color   = IM_COL32(255, 220, 50, 220);
    constexpr ImU32 grid_bounds_color = IM_COL32(0, 220, 220, 150);
    constexpr ImU32 link_color       = IM_COL32(150, 150, 150, 120);

    // --- Colliders ---
    if (vis.colliders != GizmoVisibility::None) {
        // BoxCollider
        {
            auto view = registry->view<engine::Transform, engine::physics::BoxCollider>();
            for (auto entity : view) {
                if (!should_draw(vis.colliders, entity)) continue;
                auto& t = view.get<engine::Transform>(entity);
                auto& box = view.get<engine::physics::BoxCollider>(entity);
                if (!box.enabled) continue;

                ImU32 col = box.is_trigger ? trigger_color : collider_color;

                // Match physics: compute shape in body-local space, then apply body rotation
                float abs_sx = std::abs(t.world_scale_x);
                float abs_sy = std::abs(t.world_scale_y);
                float hw = box.width * 0.5f * abs_sx;
                float hh = box.height * 0.5f * abs_sy;
                float ox = box.offset_x * t.world_scale_x;
                float oy = box.offset_y * t.world_scale_y;

                // Step 1: body-local corners rotated by collider local rotation + offset
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

                // Step 2: apply body rotation (entity world rotation) and translate to world pos
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
                if (!should_draw(vis.colliders, entity)) continue;
                auto& t = view.get<engine::Transform>(entity);
                auto& circle = view.get<engine::physics::CircleCollider>(entity);
                if (!circle.enabled) continue;

                ImU32 col = circle.is_trigger ? trigger_color : collider_color;
                float avg_scale = (std::abs(t.world_scale_x) + std::abs(t.world_scale_y)) * 0.5f;

                // Match physics: scale offset first, then rotate by entity rotation
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
                if (!should_draw(vis.colliders, entity)) continue;
                auto& t = view.get<engine::Transform>(entity);
                auto& cap = view.get<engine::physics::CapsuleCollider>(entity);
                if (!cap.enabled) continue;

                ImU32 col = cap.is_trigger ? trigger_color : collider_color;
                float avg_scale = (std::abs(t.world_scale_x) + std::abs(t.world_scale_y)) * 0.5f;
                float half_len = cap.length * 0.5f * avg_scale;
                float rad = cap.radius * avg_scale;

                // Match physics: compute in body-local space, then apply entity rotation
                float ox = cap.offset_x * t.world_scale_x;
                float oy = cap.offset_y * t.world_scale_y;

                // Capsule axis in body-local space (only collider rotation)
                float c_rot = cap.rotation * engine::DEG_TO_RAD;
                float c_cos = std::cos(c_rot);
                float c_sin = std::sin(c_rot);
                float local_ax = -c_sin;
                float local_ay = c_cos;

                // Two endpoint centers in body-local space
                float local_top_x = ox + local_ax * half_len;
                float local_top_y = oy + local_ay * half_len;
                float local_bot_x = ox - local_ax * half_len;
                float local_bot_y = oy - local_ay * half_len;

                // Apply entity rotation to transform body-local → world
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

                // Connecting lines - perpendicular direction in world space
                float total_rot = (t.world_rotation + cap.rotation) * engine::DEG_TO_RAD;
                float perp_x = std::cos(total_rot);
                float perp_y = std::sin(total_rot);
                float side_wx1 = top_wx + perp_x * rad;
                float side_wy1 = top_wy + perp_y * rad;
                float side_wx2 = bot_wx + perp_x * rad;
                float side_wy2 = bot_wy + perp_y * rad;
                draw_list->AddLine(wts(side_wx1, side_wy1),
                                   wts(side_wx2, side_wy2), col, 1.5f);
                side_wx1 = top_wx - perp_x * rad;
                side_wy1 = top_wy - perp_y * rad;
                side_wx2 = bot_wx - perp_x * rad;
                side_wy2 = bot_wy - perp_y * rad;
                draw_list->AddLine(wts(side_wx1, side_wy1),
                                   wts(side_wx2, side_wy2), col, 1.5f);
            }
        }

        // DynamicCollider (uses triangulated shapes like other colliders)
        {
            auto view = registry->view<engine::Transform, engine::physics::DynamicCollider>();
            for (auto entity : view) {
                if (!should_draw(vis.colliders, entity)) continue;
                auto& t = view.get<engine::Transform>(entity);
                auto& dc = view.get<engine::physics::DynamicCollider>(entity);
                if (!dc.enabled) continue;

                // Get or generate debug contours
                auto& loader = m_context.pixel_grid_loader();
                const DebugContours* debug = loader.get_debug_contours(entity);
                if (!debug) {
                    // Generate contours if not cached
                    loader.regenerate_debug_contours(entity, dc.simplification);
                    debug = loader.get_debug_contours(entity);
                }
                if (!debug || debug->contours.empty()) continue;

                ImU32 col = dc.is_trigger ? trigger_color : collider_color;

                float abs_sx = std::abs(t.world_scale_x);
                float abs_sy = std::abs(t.world_scale_y);
                float e_rot = t.world_rotation * engine::DEG_TO_RAD;
                float e_cos = std::cos(e_rot);
                float e_sin = std::sin(e_rot);

                // Grid height for Y-flip (grid Y=0 at top, world Y increases upward)
                float grid_height = static_cast<float>(debug->height);
                float origin_x = static_cast<float>(debug->origin_x);
                float origin_y = static_cast<float>(debug->origin_y);

                for (const auto& contour : debug->contours) {
                    if (contour.vertices.size() < 3) continue;

                    std::vector<ImVec2> screen_pts;
                    screen_pts.reserve(contour.vertices.size());

                    for (const auto& v : contour.vertices) {
                        // Convert grid coords to local coords (matching physics formula)
                        // Grid Y=0 is at top, so flip: local_y = (height - grid_y - origin_y)
                        float lx = (v.x - origin_x) * abs_sx + dc.offset_x * t.world_scale_x;
                        float ly = (grid_height - v.y - origin_y) * abs_sy + dc.offset_y * t.world_scale_y;

                        // Apply entity rotation and translate to world position
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

    // --- Terrain Colliders (play mode only) ---
    if (vis.terrain_colliders != GizmoVisibility::None && m_context.is_playing()) {
        auto* rt = m_context.runtime();
        if (rt) {
            constexpr ImU32 terrain_col = IM_COL32(0, 200, 220, 180);
            for (auto& state : rt->sim_surfaces()) {
                if (!state->terrain_colliders) continue;
                if (!should_draw(vis.terrain_colliders, state->entity)) continue;

                for (auto& [coord, chunk] : state->terrain_colliders->terrain_chunks()) {
                    if (!chunk.active) continue;
                    for (auto& verts : chunk.debug_verts) {
                        if (verts.size() < 3) continue;
                        // Convert world-space vertices to screen and draw closed polyline
                        std::vector<ImVec2> screen_pts(verts.size());
                        for (size_t i = 0; i < verts.size(); i++) {
                            screen_pts[i] = wts(verts[i].x, verts[i].y);
                        }
                        draw_list->AddPolyline(screen_pts.data(),
                                               static_cast<int>(screen_pts.size()),
                                               terrain_col, ImDrawFlags_Closed, 1.0f);
                    }
                }
            }
        }
    }

    // --- Object Origin ---
    if (vis.object_origin != GizmoVisibility::None) {
        auto view = registry->view<engine::Transform>();
        for (auto entity : view) {
            if (!should_draw(vis.object_origin, entity)) continue;
            auto& t = view.get<engine::Transform>(entity);
            ImVec2 center = wts(t.world_x, t.world_y);
            constexpr float cross = 6.0f;
            draw_list->AddLine(ImVec2(center.x - cross, center.y),
                               ImVec2(center.x + cross, center.y), origin_color, 1.0f);
            draw_list->AddLine(ImVec2(center.x, center.y - cross),
                               ImVec2(center.x, center.y + cross), origin_color, 1.0f);
        }
    }

    // --- Object Name ---
    if (vis.object_name != GizmoVisibility::None) {
        auto view = registry->view<engine::Transform, EntityInfo>();
        for (auto entity : view) {
            if (!should_draw(vis.object_name, entity)) continue;
            auto& t = view.get<engine::Transform>(entity);
            auto& info = view.get<EntityInfo>(entity);
            ImVec2 screen_pos = wts(t.world_x, t.world_y);
            ImVec2 text_size = ImGui::CalcTextSize(info.name.c_str());
            draw_list->AddText(ImVec2(screen_pos.x - text_size.x * 0.5f, screen_pos.y - 20.0f),
                               name_color, info.name.c_str());
        }
    }

    // --- Camera Bounds ---
    if (vis.camera_bounds != GizmoVisibility::None) {
        auto view = registry->view<engine::Transform, engine::render::Camera2D>();
        for (auto entity : view) {
            if (!should_draw(vis.camera_bounds, entity)) continue;
            auto& t = view.get<engine::Transform>(entity);
            auto& cam = view.get<engine::render::Camera2D>(entity);
            if (!cam.enabled) continue;

            // Use the viewport panel size as reference (matches what the game camera would see)
            float ref_w = wts.viewport_width;
            float ref_h = wts.viewport_height;
            float half_w = cam.visible_width(ref_w) * 0.5f;
            float half_h = cam.visible_height(ref_h) * 0.5f;

            // Camera bounds rectangle
            ImVec2 tl = wts(t.world_x - half_w, t.world_y + half_h);
            ImVec2 br = wts(t.world_x + half_w, t.world_y - half_h);
            draw_list->AddRect(tl, br, camera_color, 0.0f, 0, 1.5f);

            // Corner brackets (fixed screen-space size, always visible when corners are in view)
            constexpr float bracket_len = 16.0f;
            // Top-left
            draw_list->AddLine(tl, ImVec2(tl.x + bracket_len, tl.y), camera_color, 2.0f);
            draw_list->AddLine(tl, ImVec2(tl.x, tl.y + bracket_len), camera_color, 2.0f);
            // Top-right
            draw_list->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x - bracket_len, tl.y), camera_color, 2.0f);
            draw_list->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y + bracket_len), camera_color, 2.0f);
            // Bottom-right
            draw_list->AddLine(br, ImVec2(br.x - bracket_len, br.y), camera_color, 2.0f);
            draw_list->AddLine(br, ImVec2(br.x, br.y - bracket_len), camera_color, 2.0f);
            // Bottom-left
            draw_list->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x + bracket_len, br.y), camera_color, 2.0f);
            draw_list->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x, br.y - bracket_len), camera_color, 2.0f);

            // Camera icon at entity position (small diamond, always visible)
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
            draw_list->AddQuad(diamond[0], diamond[1], diamond[2], diamond[3], camera_color, 1.5f);

            // Label at entity position
            draw_list->AddText(ImVec2(center.x + icon_size + 4, center.y - 7), camera_color, "Camera");
        }
    }

    // --- Rigidbody Velocity (play mode only) ---
    if (vis.rigidbody_velocity != GizmoVisibility::None && m_context.is_playing()) {
        auto* rt = m_context.runtime();
        auto* physics = rt ? rt->physics_world() : nullptr;
        if (physics) {
            auto view = registry->view<engine::Transform, engine::physics::Rigidbody>();
            for (auto entity : view) {
                if (!should_draw(vis.rigidbody_velocity, entity)) continue;
                auto& t = view.get<engine::Transform>(entity);
                auto& rb = view.get<engine::physics::Rigidbody>(entity);
                if (!rb.enabled || !b2Body_IsValid(rb.body_id)) continue;

                b2Vec2 vel = physics->get_body_linear_velocity(rb.body_id);
                float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
                if (speed < 0.5f) continue;  // Skip negligible velocity

                // Scale arrow length (cap at reasonable screen size)
                float arrow_scale = 0.1f;
                float arrow_len = std::min(speed * arrow_scale, 100.0f);
                float nx = vel.x / speed;
                float ny = vel.y / speed;

                ImVec2 origin_s = wts(t.world_x, t.world_y);
                // In screen space: +world_x → +screen_x, +world_y → -screen_y
                ImVec2 tip_s = ImVec2(origin_s.x + nx * arrow_len * wts.zoom,
                                      origin_s.y - ny * arrow_len * wts.zoom);

                draw_list->AddLine(origin_s, tip_s, velocity_color, 2.0f);

                // Arrow head
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
                    draw_list->AddTriangleFilled(tip_s, h1, h2, velocity_color);
                }
            }
        }
    }

    // --- Pixel Grid Bounds ---
    if (vis.pixel_grid_bounds != GizmoVisibility::None) {
        auto view = registry->view<engine::Transform, engine::simulation::PixelGridComponent>();
        for (auto entity : view) {
            if (!should_draw(vis.pixel_grid_bounds, entity)) continue;
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
            draw_list->AddQuad(pts[0], pts[1], pts[2], pts[3], grid_bounds_color, 1.5f);
        }
    }

    // --- Parent-Child Links ---
    if (vis.parent_child_links != GizmoVisibility::None) {
        auto view = registry->view<engine::Transform, Hierarchy>();
        for (auto entity : view) {
            auto& hierarchy = view.get<Hierarchy>(entity);
            if (hierarchy.parent == entt::null) continue;
            if (!should_draw(vis.parent_child_links, entity)) continue;
            if (!registry->valid(hierarchy.parent) ||
                !registry->all_of<engine::Transform>(hierarchy.parent)) continue;

            auto& child_t = view.get<engine::Transform>(entity);
            auto& parent_t = registry->get<engine::Transform>(hierarchy.parent);
            ImVec2 child_s = wts(child_t.world_x, child_t.world_y);
            ImVec2 parent_s = wts(parent_t.world_x, parent_t.world_y);
            draw_list->AddLine(parent_s, child_s, link_color, 1.0f);
        }
    }
}

void ViewportPanel::handle_input() {
    ImGuiIO& io = ImGui::GetIO();
    auto& camera = m_context.viewport().camera;

    // Get viewport info for hit detection
    ImVec2 viewport_pos = ImGui::GetItemRectMin();
    ImVec2 viewport_size = ImGui::GetItemRectSize();

    // Click-to-select with left mouse button (when not manipulating or hovering gizmo)
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !m_gizmo_renderer.is_active() && !m_gizmo_renderer.is_hovering()) {
        ImVec2 mouse_pos = io.MousePos;

        auto* registry = m_context.registry();
        if (registry) {
            HitResult hit = EntityHitDetector::hit_test(
                *registry,
                mouse_pos,
                viewport_pos,
                viewport_size,
                camera.x,
                camera.y,
                camera.zoom
            );

            EntityHitDetector::process_click_selection(
                m_context, hit, mouse_pos, m_click_cycle_state,
                io.KeyCtrl, io.KeyShift
            );
        }
    }

    // Zoom with scroll wheel
    if (io.MouseWheel != 0.0f) {
        float zoom_factor = 1.1f;
        if (io.MouseWheel > 0) {
            camera.zoom *= zoom_factor;
        } else {
            camera.zoom /= zoom_factor;
        }
        camera.zoom = std::clamp(camera.zoom, 0.1f, 10.0f);
    }

    // Pan with middle mouse button
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        m_is_panning = true;
        m_pan_start_x = io.MousePos.x;
        m_pan_start_y = io.MousePos.y;
    }

    if (m_is_panning) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            float dx = io.MousePos.x - m_pan_start_x;
            float dy = io.MousePos.y - m_pan_start_y;
            camera.x -= dx / camera.zoom;
            camera.y += dy / camera.zoom;  // Flip Y
            m_pan_start_x = io.MousePos.x;
            m_pan_start_y = io.MousePos.y;
        } else {
            m_is_panning = false;
        }
    }

    // Reset camera with Home key
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        camera.x = 0.0f;
        camera.y = 0.0f;
        camera.zoom = 1.0f;
    }
}

}
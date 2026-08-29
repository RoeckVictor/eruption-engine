#include "ViewportPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/EditorPixelGridLoader.h"
#include "editor/core/RuntimeContext.h"
#include "editor/core/SimulationPlayback.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/render/SceneRenderUtils.h"
#include "editor/render/EntityHitDetector.h"
#include "editor/render/DebugOverlayRenderer.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Transform.h"
#include "engine/core/ScreenRect.h"
#include "engine/core/Logger.h"
#include "engine/core/Engine.h"
#include "engine/platform/IImGuiBackend.h"
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
#include <filesystem>
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
    ImTextureID texture_id = m_imgui_texture_id ? (ImTextureID)m_imgui_texture_id : 0;

    // UV coordinates depend on texture origin convention
    auto* device = engine::rhi::get_current_device();
    bool flip_y = device && !device->uv_origin_top_left();
    ImVec2 uv0 = flip_y ? ImVec2(0, 1) : ImVec2(0, 0);
    ImVec2 uv1 = flip_y ? ImVec2(1, 0) : ImVec2(1, 1);
    ImGui::Image(texture_id, size, uv0, uv1);

    // Get viewport rect for gizmos and overlay
    ImVec2 viewport_pos = ImGui::GetItemRectMin();
    ImVec2 viewport_size = ImGui::GetItemRectSize();

    // Handle prefab drag-drop onto viewport
    if (ImGui::BeginDragDropTarget()) {
        auto& camera = m_context.viewport().camera;

        // Peek for feedback
        if (const ImGuiPayload* peek = ImGui::GetDragDropPayload()) {
            if (peek->IsDataType("ASSET_PATH")) {
                std::string path(static_cast<const char*>(peek->Data));
                std::filesystem::path fs_path(path);
                if (fs_path.extension() == ".prefab") {
                    if (SceneSerializer::is_screen_prefab(fs_path)) {
                        ImGui::SetTooltip("Cannot drop screen prefab in world viewport");
                    }
                }
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string path(static_cast<const char*>(payload->Data));
            std::filesystem::path fs_path(path);
            if (fs_path.extension() == ".prefab") {
                if (!SceneSerializer::is_screen_prefab(fs_path)) {
                    auto* registry = m_context.registry();
                    if (registry) {
                        // Calculate world position from mouse position
                        ImVec2 mouse = ImGui::GetMousePos();
                        CoordinateTransform wts(viewport_pos, viewport_size, camera.x, camera.y, camera.zoom);
                        ImVec2 world_pos = wts.screen_to_world(mouse.x, mouse.y);

                        // Load prefab
                        SceneSerializer serializer(*registry);
                        entt::entity e = serializer.load_prefab(fs_path);
                        if (e != entt::null) {
                            // Set position
                            if (registry->all_of<engine::Transform>(e)) {
                                auto& t = registry->get<engine::Transform>(e);
                                t.x = world_pos.x;
                                t.y = world_pos.y;
                            }
                            m_context.selection().select(e);
                            m_context.scene_state().mark_dirty();
                        }
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

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
        return;
    }

    // Register the color attachment as an ImGui texture
    auto* imgui_backend = engine::platform::get_current_imgui_backend();
    if (imgui_backend && m_framebuffer->color_attachment(0)) {
        m_imgui_texture_id = imgui_backend->register_texture(m_framebuffer->color_attachment(0));
    }
}

void ViewportPanel::destroy_framebuffer() {
    if (m_imgui_texture_id) {
        auto* imgui_backend = engine::platform::get_current_imgui_backend();
        if (imgui_backend) {
            imgui_backend->unregister_texture(m_imgui_texture_id);
        }
        m_imgui_texture_id = nullptr;
    }
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
    {
        DebugOverlayConfig overlay_config;
        overlay_config.registry = registry;
        overlay_config.visibility = &m_context.gizmo_visibility();
        overlay_config.transform = &wts;
        overlay_config.draw_list = draw_list;
        overlay_config.should_draw = [this](GizmoVisibility v, entt::entity e) -> bool {
            if (v == GizmoVisibility::None) return false;
            if (v == GizmoVisibility::All) return true;
            return m_context.selection().is_selected(e);
        };
        overlay_config.pixel_grid_loader = &m_context.pixel_grid_loader();
        overlay_config.runtime = m_context.runtime();
        overlay_config.is_playing = m_context.is_playing();
        DebugOverlayRenderer::render(overlay_config);
    }

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
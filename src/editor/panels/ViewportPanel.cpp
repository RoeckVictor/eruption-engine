#include "ViewportPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"

#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace editor {

ViewportPanel::ViewportPanel(EditorContext& context)
    : Panel("Viewport")
    , m_context(context)
    , m_gizmo_renderer(context)
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
    // Get available size
    ImVec2 size = ImGui::GetContentRegionAvail();
    int width = static_cast<int>(size.x);
    int height = static_cast<int>(size.y);

    if (width <= 0 || height <= 0) {
        return;
    }

    // Recreate framebuffer if size changed
    if (width != m_viewport_width || height != m_viewport_height) {
        destroy_framebuffer();
        create_framebuffer(width, height);
    }

    // Render scene to framebuffer
    render_scene();

    // Display the framebuffer texture
    ImGui::Image(
        (ImTextureID)(uintptr_t)m_texture,
        size,
        ImVec2(0, 1),  // UV flipped for OpenGL
        ImVec2(1, 0)
    );

    // Get viewport rect for gizmos and overlay
    ImVec2 viewport_pos = ImGui::GetItemRectMin();
    ImVec2 viewport_size = ImGui::GetItemRectSize();

    // Handle input when hovered (but not when gizmo is active)
    if (ImGui::IsItemHovered() && !m_gizmo_renderer.is_active()) {
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

    // Create framebuffer
    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    // Create color texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

    // Create depth buffer
    glGenRenderbuffers(1, &m_depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depth_buffer);

    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // Log error
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ViewportPanel::destroy_framebuffer() {
    if (m_framebuffer) {
        glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    if (m_depth_buffer) {
        glDeleteRenderbuffers(1, &m_depth_buffer);
        m_depth_buffer = 0;
    }

    m_viewport_width = 0;
    m_viewport_height = 0;
}

void ViewportPanel::render_scene() {
    if (!m_framebuffer) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_viewport_width, m_viewport_height);

    // Clear with a dark color
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render grid
    render_grid();

    // TODO: Render actual scene entities using the registry
    // For now, entities are represented in the hierarchy

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ViewportPanel::render_grid() {
    // Grid will be drawn in the overlay using ImGui draw list
    // since we need screen-space coordinates
}

void ViewportPanel::render_overlay() {
    // Get viewport position
    ImVec2 pos = ImGui::GetItemRectMin();
    ImVec2 size = ImGui::GetItemRectSize();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto& camera = m_context.camera();

    // Draw grid if enabled
    if (m_context.is_grid_visible()) {
        float grid_size = m_context.grid_size();

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

        // Screen center (where camera.x, camera.y is displayed)
        float screen_center_x = pos.x + size.x * 0.5f;
        float screen_center_y = pos.y + size.y * 0.5f;

        // Lambda to convert world to screen coordinates
        auto world_to_screen = [&](float wx, float wy) -> ImVec2 {
            float sx = screen_center_x + (wx - camera.x) * camera.zoom;
            float sy = screen_center_y - (wy - camera.y) * camera.zoom;  // Flip Y
            return ImVec2(sx, sy);
        };

        // Draw vertical lines
        for (float x = start_x; x <= world_right; x += grid_size) {
            ImVec2 top = world_to_screen(x, world_top);
            ImVec2 bottom = world_to_screen(x, world_bottom);

            // Major lines every 5 grid units
            bool is_major = (static_cast<int>(std::round(x / grid_size)) % 5) == 0;
            ImU32 color = is_major ? major_grid_color : grid_color;

            draw_list->AddLine(top, bottom, color);
        }

        // Draw horizontal lines
        for (float y = start_y; y <= world_top; y += grid_size) {
            ImVec2 left = world_to_screen(world_left, y);
            ImVec2 right = world_to_screen(world_right, y);

            // Major lines every 5 grid units
            bool is_major = (static_cast<int>(std::round(y / grid_size)) % 5) == 0;
            ImU32 color = is_major ? major_grid_color : grid_color;

            draw_list->AddLine(left, right, color);
        }

        // Draw origin axes (thicker, colored)
        ImVec2 origin = world_to_screen(0, 0);

        // X axis (red) - only if origin is in view
        if (world_bottom <= 0 && world_top >= 0) {
            ImVec2 x_start = world_to_screen(world_left, 0);
            ImVec2 x_end = world_to_screen(world_right, 0);
            draw_list->AddLine(x_start, x_end, IM_COL32(180, 80, 80, 200), 1.5f);
        }

        // Y axis (green)
        if (world_left <= 0 && world_right >= 0) {
            ImVec2 y_start = world_to_screen(0, world_bottom);
            ImVec2 y_end = world_to_screen(0, world_top);
            draw_list->AddLine(y_start, y_end, IM_COL32(80, 180, 80, 200), 1.5f);
        }
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
    if (!m_context.is_grid_visible()) {
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
    if (m_context.is_dirty()) {
        draw_list->AddText(
            ImVec2(pos.x + size.x - 80, pos.y + 10),
            IM_COL32(255, 200, 100, 255),
            "* Unsaved"
        );
    }

    // Draw selection count
    if (!m_context.selection().empty()) {
        char sel_info[64];
        snprintf(sel_info, sizeof(sel_info), "Selected: %zu", m_context.selection().size());
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
    auto& camera = m_context.camera();

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

} // namespace editor

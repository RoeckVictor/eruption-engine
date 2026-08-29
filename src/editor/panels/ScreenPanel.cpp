#include "ScreenPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/RuntimeContext.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/render/SceneRenderUtils.h"
#include "engine/core/ScreenRect.h"
#include "engine/core/Logger.h"
#include "engine/core/Engine.h"
#include "engine/platform/Input.h"
#include "engine/platform/IImGuiBackend.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"
#include "engine/asset/VFS.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>

namespace editor {

// Define static constexpr array outside class for ODR compliance
constexpr RefResolution ScreenPanel::RESOLUTIONS[];

ScreenPanel::ScreenPanel(EditorContext& context)
    : Panel("Screen")
    , m_context(context)
{
}

ScreenPanel::~ScreenPanel() {
    destroy_framebuffer();
}

void ScreenPanel::on_open() {
    // Will fit to canvas once framebuffer is created
    m_canvas.zoom = 0.0f;  // Signal to fit on first frame
}

void ScreenPanel::on_close() {
    destroy_framebuffer();
}

void ScreenPanel::on_gui() {
    // When the screen panel is focused, clear any editing override
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        m_context.clear_editing_override();
    }

    // Render toolbar with resolution dropdown
    render_toolbar();

    // Get available size after toolbar
    ImVec2 size = ImGui::GetContentRegionAvail();
    int width = static_cast<int>(size.x);
    int height = static_cast<int>(size.y);

    if (width <= 0 || height <= 0) {
        return;
    }

    // Debounced framebuffer resize
    if (m_resize_debouncer.should_resize(m_canvas_width, m_canvas_height, width, height, ImGui::GetIO().DeltaTime)) {
        bool was_first_create = (m_canvas_width == 0);
        destroy_framebuffer();
        create_framebuffer(m_resize_debouncer.target_width(), m_resize_debouncer.target_height());
        // Fit to canvas on first creation
        if (was_first_create || m_canvas.zoom <= 0.0f) {
            fit_to_canvas();
        }
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

    // Get canvas rect
    ImVec2 canvas_pos = ImGui::GetItemRectMin();
    ImVec2 canvas_size = ImGui::GetItemRectSize();

    // Handle prefab drag-drop onto screen panel
    if (ImGui::BeginDragDropTarget()) {
        // Peek for feedback
        if (const ImGuiPayload* peek = ImGui::GetDragDropPayload()) {
            if (peek->IsDataType("ASSET_PATH")) {
                std::string path(static_cast<const char*>(peek->Data));
                std::filesystem::path fs_path(path);
                if (fs_path.extension() == ".prefab") {
                    if (!SceneSerializer::is_screen_prefab(fs_path)) {
                        ImGui::SetTooltip("Cannot drop world prefab in screen panel");
                    }
                }
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string path(static_cast<const char*>(payload->Data));
            std::filesystem::path fs_path(path);
            if (fs_path.extension() == ".prefab") {
                if (SceneSerializer::is_screen_prefab(fs_path)) {
                    auto* registry = m_context.registry();
                    if (registry) {
                        // Calculate screen position from mouse position
                        ImVec2 mouse = ImGui::GetMousePos();
                        ImVec2 local_mouse = ImVec2(mouse.x - canvas_pos.x, mouse.y - canvas_pos.y);
                        float screen_x, screen_y;
                        m_canvas.to_screen(local_mouse, canvas_size, screen_x, screen_y);

                        // Load prefab
                        SceneSerializer serializer(*registry);
                        entt::entity e = serializer.load_prefab(fs_path);
                        if (e != entt::null) {
                            // Set position via offset (offset is relative to anchor position)
                            if (registry->all_of<engine::ScreenRect>(e)) {
                                auto& rect = registry->get<engine::ScreenRect>(e);
                                rect.offset_x = screen_x - rect.anchor_x * m_canvas.ref_width + rect.pivot_x * rect.width;
                                rect.offset_y = screen_y - rect.anchor_y * m_canvas.ref_height + rect.pivot_y * rect.height;
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

    // Handle input when hovered
    if (ImGui::IsItemHovered()) {
        handle_input(canvas_pos, canvas_size);
    }

    // Render overlay (entities and gizmos)
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    render_entities(draw_list, canvas_pos, canvas_size);
    render_gizmos(draw_list, canvas_pos, canvas_size);

    // Draw zoom info
    char info[128];
    snprintf(info, sizeof(info), "Zoom: %.0f%%", m_canvas.zoom * 100.0f);
    draw_list->AddText(
        ImVec2(canvas_pos.x + 10, canvas_pos.y + 10),
        IM_COL32(200, 200, 200, 200),
        info
    );

    // Draw selection count for screen entities
    int screen_selected = 0;
    auto* registry = m_context.registry();
    if (registry) {
        for (auto entity : m_context.selection().selection()) {
            if (is_screen_space_entity(*registry, entity)) {
                screen_selected++;
            }
        }
    }
    if (screen_selected > 0) {
        char sel_info[64];
        snprintf(sel_info, sizeof(sel_info), "Selected: %d screen entities", screen_selected);
        draw_list->AddText(
            ImVec2(canvas_pos.x + 10, canvas_pos.y + 30),
            IM_COL32(255, 180, 100, 200),
            sel_info
        );
    }
}

void ScreenPanel::render_toolbar() {
    // Resolution dropdown
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("##RefResolution", RESOLUTIONS[m_resolution_index].name)) {
        for (int i = 0; i < RESOLUTION_COUNT; i++) {
            bool is_selected = (m_resolution_index == i);
            if (ImGui::Selectable(RESOLUTIONS[i].name, is_selected)) {
                m_resolution_index = i;
                m_canvas.ref_width = RESOLUTIONS[i].width;
                m_canvas.ref_height = RESOLUTIONS[i].height;
                fit_to_canvas();
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reference resolution for screen entities");
    }

    ImGui::SameLine();

    // Fit to canvas button
    if (ImGui::Button("Fit")) {
        fit_to_canvas();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Fit reference rect to canvas");
    }

    ImGui::SameLine();

    // Reset view button (1:1 pixel mode)
    if (ImGui::Button("1:1")) {
        m_canvas.reset();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("View at 100%% zoom (1 pixel = 1 pixel)");
    }

    ImGui::SameLine();

    // Reset pan only
    if (ImGui::Button("Center")) {
        m_canvas.pan_x = 0.0f;
        m_canvas.pan_y = 0.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Center view (Home)");
    }

    ImGui::Separator();
}

void ScreenPanel::create_framebuffer(int width, int height) {
    m_canvas_width = width;
    m_canvas_height = height;

    auto* runtime = m_context.runtime();
    if (!runtime) {
        m_resize_debouncer.set_failed();
        return;
    }

    auto* eng = runtime->engine();
    if (!eng) {
        m_resize_debouncer.set_failed();
        return;
    }

    auto* device = eng->rhi_device();
    if (!device) {
        m_resize_debouncer.set_failed();
        return;
    }

    m_framebuffer = device->create_simple_framebuffer(
        width, height,
        engine::rhi::TextureFormat::RGBA8,
        true
    );

    if (!m_framebuffer) {
        engine::Logger::instance().error("ScreenPanel", "Failed to create framebuffer");
        m_resize_debouncer.set_failed();
        return;
    }

    auto* imgui_backend = engine::platform::get_current_imgui_backend();
    if (imgui_backend && m_framebuffer->color_attachment(0)) {
        m_imgui_texture_id = imgui_backend->register_texture(m_framebuffer->color_attachment(0));
    }
}

void ScreenPanel::destroy_framebuffer() {
    if (m_imgui_texture_id) {
        auto* imgui_backend = engine::platform::get_current_imgui_backend();
        if (imgui_backend) imgui_backend->unregister_texture(m_imgui_texture_id);
        m_imgui_texture_id = nullptr;
    }
    m_framebuffer.reset();
    m_canvas_width = 0;
    m_canvas_height = 0;
}

void ScreenPanel::render_scene() {
    if (!m_framebuffer) {
        return;
    }

    auto* runtime = m_context.runtime();
    if (!runtime) return;

    auto* eng = runtime->engine();
    if (!eng) return;

    auto* device = eng->rhi_device();
    if (!device) return;

    auto* ctx = device->context();
    ctx->bind_framebuffer(m_framebuffer.get());
    ctx->set_viewport(0, 0, m_canvas_width, m_canvas_height);

    // Dark background with subtle grid pattern suggestion
    ctx->clear(0.12f, 0.12f, 0.15f, 1.0f);
    ctx->clear_depth(1.0f);

    ctx->bind_framebuffer(nullptr);
}

void ScreenPanel::render_entities(ImDrawList* draw_list, ImVec2 canvas_pos, ImVec2 canvas_size) {
    auto* registry = m_context.registry();
    if (!registry) return;

    // Update screen rects using reference resolution
    update_screen_rects(*registry, m_canvas.ref_width, m_canvas.ref_height);

    // Draw reference screen bounds
    ImVec2 ref_tl = m_canvas.to_canvas(0, 0, canvas_pos, canvas_size);
    ImVec2 ref_br = m_canvas.to_canvas(m_canvas.ref_width, m_canvas.ref_height, canvas_pos, canvas_size);
    draw_list->AddRect(ref_tl, ref_br, IM_COL32(100, 100, 100, 150), 0.0f, 0, 2.0f);

    // Collect and sort entities by layer
    ensure_text_renderer();
    auto entries = collect_screen_renderables(*registry);

    for (const auto& entry : entries) {
        auto& rect = registry->get<engine::ScreenRect>(entry.entity);
        bool is_selected = m_context.selection().is_selected(entry.entity);

        if (entry.has_image) {
            render_screen_image(draw_list, *registry, m_image_textures, m_canvas,
                                entry.entity, canvas_pos, canvas_size);
        }

        if (entry.has_text) {
            render_screen_text(draw_list, *registry, m_text_renderer.get(), m_canvas,
                               entry.entity, canvas_pos, canvas_size);
        }

        // Placeholder for entities without Image or Text
        if (!entry.has_image && !entry.has_text) {
            ImVec2 tl = m_canvas.to_canvas(rect.computed_x, rect.computed_y, canvas_pos, canvas_size);
            ImVec2 br = m_canvas.to_canvas(
                rect.computed_x + rect.computed_width,
                rect.computed_y + rect.computed_height,
                canvas_pos, canvas_size
            );

            ImU32 fill_color = is_selected ? IM_COL32(100, 150, 200, 60) : IM_COL32(80, 80, 100, 40);
            ImU32 border_color = is_selected ? IM_COL32(100, 180, 255, 255) : IM_COL32(120, 120, 140, 180);

            draw_list->AddRectFilled(tl, br, fill_color);
            draw_list->AddRect(tl, br, border_color, 0.0f, 0, is_selected ? 2.0f : 1.0f);
        }

        if (is_selected) {
            ImVec2 tl = m_canvas.to_canvas(rect.computed_x, rect.computed_y, canvas_pos, canvas_size);
            ImVec2 br = m_canvas.to_canvas(
                rect.computed_x + rect.computed_width,
                rect.computed_y + rect.computed_height,
                canvas_pos, canvas_size
            );

            draw_list->AddRect(tl, br, IM_COL32(100, 180, 255, 255), 0.0f, 0, 2.0f);

            ImVec2 anchor_pos = m_canvas.to_canvas(
                rect.computed_x + rect.pivot_x * rect.computed_width,
                rect.computed_y + rect.pivot_y * rect.computed_height,
                canvas_pos, canvas_size
            );
            draw_list->AddCircleFilled(anchor_pos, 4.0f, IM_COL32(255, 200, 100, 200));
            draw_list->AddCircle(anchor_pos, 4.0f, IM_COL32(255, 255, 255, 255), 12, 1.0f);
        }
    }
}

void ScreenPanel::render_gizmos(ImDrawList* draw_list, ImVec2 canvas_pos, ImVec2 canvas_size) {
    auto* registry = m_context.registry();
    if (!registry) return;

    // Draw resize handles for selected screen entities
    for (auto entity : m_context.selection().selection()) {
        if (!is_screen_space_entity(*registry, entity)) continue;
        if (!registry->all_of<engine::ScreenRect>(entity)) continue;

        auto& rect = registry->get<engine::ScreenRect>(entity);

        ImVec2 tl = m_canvas.to_canvas(rect.computed_x, rect.computed_y, canvas_pos, canvas_size);
        ImVec2 br = m_canvas.to_canvas(
            rect.computed_x + rect.computed_width,
            rect.computed_y + rect.computed_height,
            canvas_pos, canvas_size
        );

        constexpr float handle_size = 6.0f;
        ImU32 handle_color = IM_COL32(255, 255, 255, 255);
        ImU32 handle_fill = IM_COL32(100, 150, 255, 200);

        // Corner handles
        ImVec2 corners[4] = {tl, ImVec2(br.x, tl.y), br, ImVec2(tl.x, br.y)};
        for (int i = 0; i < 4; i++) {
            draw_list->AddRectFilled(
                ImVec2(corners[i].x - handle_size, corners[i].y - handle_size),
                ImVec2(corners[i].x + handle_size, corners[i].y + handle_size),
                handle_fill
            );
            draw_list->AddRect(
                ImVec2(corners[i].x - handle_size, corners[i].y - handle_size),
                ImVec2(corners[i].x + handle_size, corners[i].y + handle_size),
                handle_color
            );
        }

        // Edge midpoint handles
        ImVec2 mid_top((tl.x + br.x) * 0.5f, tl.y);
        ImVec2 mid_bot((tl.x + br.x) * 0.5f, br.y);
        ImVec2 mid_left(tl.x, (tl.y + br.y) * 0.5f);
        ImVec2 mid_right(br.x, (tl.y + br.y) * 0.5f);

        ImVec2 edges[4] = {mid_top, mid_right, mid_bot, mid_left};
        for (int i = 0; i < 4; i++) {
            draw_list->AddRectFilled(
                ImVec2(edges[i].x - handle_size * 0.7f, edges[i].y - handle_size * 0.7f),
                ImVec2(edges[i].x + handle_size * 0.7f, edges[i].y + handle_size * 0.7f),
                handle_fill
            );
            draw_list->AddRect(
                ImVec2(edges[i].x - handle_size * 0.7f, edges[i].y - handle_size * 0.7f),
                ImVec2(edges[i].x + handle_size * 0.7f, edges[i].y + handle_size * 0.7f),
                handle_color
            );
        }
    }
}

void ScreenPanel::handle_input(ImVec2 canvas_pos, ImVec2 canvas_size) {
    ImGuiIO& io = ImGui::GetIO();
    auto* registry = m_context.registry();
    if (!registry) return;

    ImVec2 mouse_pos = io.MousePos;
    ImVec2 local_mouse(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

    float screen_x, screen_y;
    m_canvas.to_screen(local_mouse, canvas_size, screen_x, screen_y);

    // Zoom with mouse wheel
    if (io.MouseWheel != 0.0f) {
        float zoom_factor = 1.1f;
        if (io.MouseWheel > 0) {
            m_canvas.zoom *= zoom_factor;
        } else {
            m_canvas.zoom /= zoom_factor;
        }
        m_canvas.zoom = std::clamp(m_canvas.zoom, 0.1f, 10.0f);
    }

    // Reset with Home key
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        reset_camera();
    }

    // Pan with middle mouse button
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        m_is_panning = true;
        m_pan_start_mouse_x = io.MousePos.x;
        m_pan_start_mouse_y = io.MousePos.y;
        m_pan_start_offset_x = m_canvas.pan_x;
        m_pan_start_offset_y = m_canvas.pan_y;
    }

    if (m_is_panning) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            float scale = m_canvas.zoom;

            float dx = io.MousePos.x - m_pan_start_mouse_x;
            float dy = io.MousePos.y - m_pan_start_mouse_y;

            m_canvas.pan_x = m_pan_start_offset_x - dx / scale;
            m_canvas.pan_y = m_pan_start_offset_y - dy / scale;
        } else {
            m_is_panning = false;
        }
    }

    // Handle entity dragging (takes priority over panning)
    if (m_is_dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (registry->valid(m_drag_entity) && registry->all_of<engine::ScreenRect>(m_drag_entity)) {
                auto& rect = registry->get<engine::ScreenRect>(m_drag_entity);

                float dx = screen_x - m_drag_start_x;
                float dy = screen_y - m_drag_start_y;

                rect.offset_x = m_entity_start_offset_x + dx;
                rect.offset_y = m_entity_start_offset_y + dy;

                m_context.scene_state().mark_dirty();
            }
        } else {
            m_is_dragging = false;
            m_drag_entity = entt::null;
        }
        return;
    }

    // Click to select (only if not panning)
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_is_panning) {
        entt::entity hit_entity = entt::null;

        // Find entity under cursor (reverse order for proper z-ordering)
        auto view = registry->view<engine::ScreenRect>();
        for (auto entity : view) {
            auto& rect = view.get<engine::ScreenRect>(entity);

            if (registry->all_of<EntityInfo>(entity)) {
                if (!registry->get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }
            if (!rect.enabled) continue;

            // Check if point is inside rect
            if (screen_x >= rect.computed_x &&
                screen_x <= rect.computed_x + rect.computed_width &&
                screen_y >= rect.computed_y &&
                screen_y <= rect.computed_y + rect.computed_height) {
                hit_entity = entity;
                // Don't break - last one wins (top of stack)
            }
        }

        if (hit_entity != entt::null) {
            if (io.KeyCtrl) {
                if (m_context.selection().is_selected(hit_entity)) {
                    m_context.selection().remove_from_selection(hit_entity);
                } else {
                    m_context.selection().add_to_selection(hit_entity);
                }
            } else if (io.KeyShift) {
                m_context.selection().add_to_selection(hit_entity);
            } else {
                m_context.selection().select(hit_entity);
            }

            // Start dragging if this is selected
            if (m_context.selection().is_selected(hit_entity)) {
                m_is_dragging = true;
                m_drag_entity = hit_entity;
                m_drag_start_x = screen_x;
                m_drag_start_y = screen_y;
                auto& rect = registry->get<engine::ScreenRect>(hit_entity);
                m_entity_start_offset_x = rect.offset_x;
                m_entity_start_offset_y = rect.offset_y;
            }
        } else {
            // Clicked on empty space - deselect
            if (!io.KeyCtrl && !io.KeyShift) {
                m_context.selection().clear_selection();
            }
        }
    }
}

void ScreenPanel::ensure_text_renderer() {
    if (m_text_renderer) return;

    auto* runtime = m_context.runtime();
    if (!runtime) return;

    auto* eng = runtime->engine();
    if (!eng) return;

    m_text_renderer = std::make_unique<EditorTextRenderer>(eng->assets());
}

}
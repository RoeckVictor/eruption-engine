#include "GamePanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/render/SceneRenderUtils.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Transform.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/render/Camera2D.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace editor {

GamePanel::GamePanel(EditorContext& context)
    : Panel("Game")
    , m_context(context)
{
}

GamePanel::~GamePanel() = default;

void GamePanel::on_open() {}
void GamePanel::on_close() {}

void GamePanel::on_gui() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0 || avail.y <= 0) return;

    ImVec2 panel_pos = ImGui::GetCursorScreenPos();
    ImVec2 panel_size = avail;

    // Draw background using scene settings
    const auto& bg = m_context.scene_settings().bg_color;
    uint8_t bg_r = static_cast<uint8_t>(bg[0] * 255.0f);
    uint8_t bg_g = static_cast<uint8_t>(bg[1] * 255.0f);
    uint8_t bg_b = static_cast<uint8_t>(bg[2] * 255.0f);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(panel_pos,
        ImVec2(panel_pos.x + panel_size.x, panel_pos.y + panel_size.y),
        IM_COL32(bg_r, bg_g, bg_b, 255));

    // Reserve the space so ImGui knows it's used
    ImGui::Dummy(panel_size);

    if (!m_context.is_playing()) {
        // Centered message when not playing
        const char* msg = "Enter Play Mode to see game view";
        ImVec2 text_size = ImGui::CalcTextSize(msg);
        ImVec2 text_pos(
            panel_pos.x + (panel_size.x - text_size.x) * 0.5f,
            panel_pos.y + (panel_size.y - text_size.y) * 0.5f
        );
        draw_list->AddText(text_pos, IM_COL32(120, 120, 120, 200), msg);
        return;
    }

    // Find camera entity
    entt::entity cam_entity = find_camera_entity();
    if (cam_entity == entt::null) {
        const char* msg = "No active Camera2D in scene";
        ImVec2 text_size = ImGui::CalcTextSize(msg);
        ImVec2 text_pos(
            panel_pos.x + (panel_size.x - text_size.x) * 0.5f,
            panel_pos.y + (panel_size.y - text_size.y) * 0.5f
        );
        draw_list->AddText(text_pos, IM_COL32(200, 150, 50, 200), msg);
        return;
    }

    render_game_view(panel_pos, panel_size);
}

entt::entity GamePanel::find_camera_entity() const {
    auto* registry = m_context.registry();
    if (!registry) return entt::null;

    auto view = registry->view<engine::Transform, engine::render::Camera2D>();
    for (auto entity : view) {
        auto& cam = view.get<engine::render::Camera2D>(entity);
        if (cam.enabled) {
            return entity;
        }
    }
    return entt::null;
}

void GamePanel::render_game_view(ImVec2 panel_pos, ImVec2 panel_size) {
    auto* registry = m_context.registry();
    if (!registry) return;

    entt::entity cam_entity = find_camera_entity();
    if (cam_entity == entt::null) return;

    auto& cam_transform = registry->get<engine::Transform>(cam_entity);
    auto& cam2d = registry->get<engine::render::Camera2D>(cam_entity);

    float cam_x = cam_transform.world_x;
    float cam_y = cam_transform.world_y;
    float cam_zoom = cam2d.zoom;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    WorldToScreen wts(panel_pos, panel_size, cam_x, cam_y, cam_zoom);

    // Clip drawing to panel bounds
    draw_list->PushClipRect(panel_pos,
        ImVec2(panel_pos.x + panel_size.x, panel_pos.y + panel_size.y), true);

    // Collect renderable entities and sort by layer
    struct RenderItem {
        entt::entity entity;
        int layer;
    };
    std::vector<RenderItem> render_items;

    auto view = registry->view<engine::Transform,
                                engine::simulation::PixelGridComponent,
                                engine::render::PixelGridRenderer>();
    for (auto entity : view) {
        if (registry->all_of<EntityInfo>(entity)) {
            if (!registry->get<EntityInfo>(entity).enabled_in_hierarchy) continue;
        }

        auto& renderer = view.get<engine::render::PixelGridRenderer>(entity);
        if (!renderer.enabled) continue;

        render_items.push_back({entity, renderer.layer});
    }

    std::sort(render_items.begin(), render_items.end(),
              [](const RenderItem& a, const RenderItem& b) { return a.layer < b.layer; });

    // Render sorted entities
    for (const auto& item : render_items) {
        auto& transform = registry->get<engine::Transform>(item.entity);
        auto& grid_comp = registry->get<engine::simulation::PixelGridComponent>(item.entity);
        auto& renderer = registry->get<engine::render::PixelGridRenderer>(item.entity);

        auto quad = compute_pixel_grid_quad(transform, grid_comp, renderer, wts);
        GLuint grid_tex = resolve_grid_texture(
            item.entity, grid_comp.pixel_grid_path, m_context.runtime(), m_grid_textures);
        draw_pixel_grid_quad(draw_list, quad, grid_tex);
    }

    draw_list->PopClipRect();

    m_grid_textures.cleanup(m_context.registry());
}

} // namespace editor

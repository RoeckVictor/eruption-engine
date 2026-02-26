#include "GamePanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/RuntimeContext.h"
#include "editor/render/SceneRenderUtils.h"
#include "engine/core/Transform.h"
#include "engine/core/ScreenRect.h"
#include "engine/render/Camera2D.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace editor {

GamePanel::GamePanel(EditorContext& context)
    : Panel("Game")
    , m_context(context)
    , m_scene_renderer(context)
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

    // Update viewport info for correct screen-to-world conversion in scripts
    if (auto* runtime = m_context.runtime()) {
        ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        float vp_x = panel_pos.x - main_viewport->Pos.x;
        float vp_y = panel_pos.y - main_viewport->Pos.y;
        runtime->set_viewport(vp_x, vp_y, panel_size.x, panel_size.y);
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

void GamePanel::render_screen_image(ImDrawList* draw_list, entt::entity entity,
                                     ImVec2 panel_pos, ImVec2 panel_size) {
    auto* registry = m_context.registry();
    if (!registry) return;

    auto& rect = registry->get<engine::ScreenRect>(entity);
    auto& image = registry->get<engine::render::Image>(entity);

    if (!rect.enabled || !image.enabled) return;

    int tex_width, tex_height;
    void* texture = m_scene_renderer.image_textures().get(image.sprite_path, tex_width, tex_height);

    const auto& settings = m_context.scene_settings();
    float scale_x = panel_size.x / settings.reference_width;
    float scale_y = panel_size.y / settings.reference_height;

    ImVec2 tl(panel_pos.x + rect.computed_x * scale_x,
              panel_pos.y + rect.computed_y * scale_y);
    ImVec2 br(panel_pos.x + (rect.computed_x + rect.computed_width) * scale_x,
              panel_pos.y + (rect.computed_y + rect.computed_height) * scale_y);

    auto uv = compute_image_uv(image);
    ImU32 tint = compute_image_tint(image);

    draw_list->AddImage(
        (ImTextureID)(uintptr_t)texture,
        tl, br,
        ImVec2(uv.u0, uv.v0), ImVec2(uv.u1, uv.v1),
        tint
    );
}

void GamePanel::render_screen_text(ImDrawList* draw_list, entt::entity entity,
                                    ImVec2 panel_pos, ImVec2 panel_size) {
    auto* registry = m_context.registry();
    if (!registry) return;

    auto& rect = registry->get<engine::ScreenRect>(entity);
    auto& text = registry->get<engine::render::Text>(entity);

    if (!rect.enabled || !text.enabled) return;

    const auto& settings = m_context.scene_settings();
    float scale_x = panel_size.x / settings.reference_width;
    float scale_y = panel_size.y / settings.reference_height;

    ImVec2 pos(panel_pos.x + rect.computed_x * scale_x,
               panel_pos.y + rect.computed_y * scale_y);

    // For screen-space text, we use ImGui's default font at a scaled size
    // This is simpler than using EditorTextRenderer which is more for world-space
    ImU32 color = IM_COL32(
        static_cast<uint8_t>(text.color_r * 255.0f),
        static_cast<uint8_t>(text.color_g * 255.0f),
        static_cast<uint8_t>(text.color_b * 255.0f),
        static_cast<uint8_t>(text.color_a * 255.0f)
    );
    draw_list->AddText(pos, color, text.content.c_str());
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

    // Render world-space entities using the scene renderer (no selection outlines in game view)
    m_scene_renderer.render_world_entities(draw_list, *registry, wts, false, m_context.runtime());

    // Now render screen-space entities (on top of world-space)
    struct ScreenRenderItem {
        entt::entity entity;
        int layer;
        bool has_image;
        bool has_text;
    };
    std::vector<ScreenRenderItem> screen_items;

    {
        auto view = registry->view<engine::ScreenRect>();
        for (auto entity : view) {
            if (registry->all_of<EntityInfo>(entity)) {
                if (!registry->get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }

            auto& rect = view.get<engine::ScreenRect>(entity);
            if (!rect.enabled) continue;

            bool has_image = registry->all_of<engine::render::Image>(entity);
            bool has_text = registry->all_of<engine::render::Text>(entity);

            if (!has_image && !has_text) continue;

            int layer = 0;
            if (has_image) {
                layer = registry->get<engine::render::Image>(entity).layer;
            } else if (has_text) {
                layer = registry->get<engine::render::Text>(entity).layer;
            }

            screen_items.push_back({entity, layer, has_image, has_text});
        }
    }

    // Sort screen items by layer
    std::sort(screen_items.begin(), screen_items.end(),
              [](const ScreenRenderItem& a, const ScreenRenderItem& b) { return a.layer < b.layer; });

    // Render screen-space entities
    for (const auto& item : screen_items) {
        if (item.has_image) {
            render_screen_image(draw_list, item.entity, panel_pos, panel_size);
        }
        if (item.has_text) {
            render_screen_text(draw_list, item.entity, panel_pos, panel_size);
        }
    }

    draw_list->PopClipRect();

    m_scene_renderer.cleanup(m_context.registry());
}

}
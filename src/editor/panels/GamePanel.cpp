#include "GamePanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Transform.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/render/Camera2D.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"

#include <imgui.h>
#include <cmath>
#include <cstdint>

namespace editor {

GamePanel::GamePanel(EditorContext& context)
    : Panel("Game")
    , m_context(context)
{
}

GamePanel::~GamePanel() {
    // Clean up cached textures
    for (auto& [entity, cached] : m_grid_textures) {
        if (cached.texture_id) {
            glDeleteTextures(1, &cached.texture_id);
        }
    }
    m_grid_textures.clear();
}

void GamePanel::on_open() {}
void GamePanel::on_close() {}

void GamePanel::on_gui() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0 || avail.y <= 0) return;

    ImVec2 panel_pos = ImGui::GetCursorScreenPos();
    ImVec2 panel_size = avail;

    // Draw black background
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(panel_pos,
        ImVec2(panel_pos.x + panel_size.x, panel_pos.y + panel_size.y),
        IM_COL32(0, 0, 0, 255));

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
    float screen_center_x = panel_pos.x + panel_size.x * 0.5f;
    float screen_center_y = panel_pos.y + panel_size.y * 0.5f;

    auto world_to_screen = [&](float wx, float wy) -> ImVec2 {
        float sx = screen_center_x + (wx - cam_x) * cam_zoom;
        float sy = screen_center_y - (wy - cam_y) * cam_zoom;
        return ImVec2(sx, sy);
    };

    // Clip drawing to panel bounds
    draw_list->PushClipRect(panel_pos,
        ImVec2(panel_pos.x + panel_size.x, panel_pos.y + panel_size.y), true);

    // Render entities with Transform + PixelGridComponent + PixelGridRenderer
    auto view = registry->view<engine::Transform,
                                engine::simulation::PixelGridComponent,
                                engine::render::PixelGridRenderer>();
    for (auto entity : view) {
        auto& transform = view.get<engine::Transform>(entity);
        auto& grid_comp = view.get<engine::simulation::PixelGridComponent>(entity);
        auto& renderer = view.get<engine::render::PixelGridRenderer>(entity);

        // Skip disabled entities
        if (registry->all_of<EntityInfo>(entity)) {
            if (!registry->get<EntityInfo>(entity).enabled_in_hierarchy) {
                continue;
            }
        }

        if (!renderer.enabled) continue;

        // Compute sprite corners (same logic as ViewportPanel)
        float w = grid_comp.width > 0 ? static_cast<float>(grid_comp.width) : 32.0f;
        float h = grid_comp.height > 0 ? static_cast<float>(grid_comp.height) : 32.0f;
        float ox = static_cast<float>(grid_comp.origin_x);
        float oy = static_cast<float>(grid_comp.origin_y);
        float sx = transform.world_scale_x;
        float sy = transform.world_scale_y;
        float rot_rad = transform.world_rotation * engine::DEG_TO_RAD;
        float cos_r = std::cos(rot_rad);
        float sin_r = std::sin(rot_rad);

        float lx0 = -ox * sx,       ly0 = (h - oy) * sy;
        float lx1 = (w - ox) * sx,  ly1 = (h - oy) * sy;
        float lx2 = (w - ox) * sx,  ly2 = -oy * sy;
        float lx3 = -ox * sx,       ly3 = -oy * sy;

        auto rotate_to_screen = [&](float lx, float ly) -> ImVec2 {
            float wx = transform.world_x + lx * cos_r - ly * sin_r;
            float wy = transform.world_y + lx * sin_r + ly * cos_r;
            return world_to_screen(wx, wy);
        };

        ImVec2 p0 = rotate_to_screen(lx0, ly0);
        ImVec2 p1 = rotate_to_screen(lx1, ly1);
        ImVec2 p2 = rotate_to_screen(lx2, ly2);
        ImVec2 p3 = rotate_to_screen(lx3, ly3);

        // Get texture — prefer live simulation texture during play mode
        GLuint grid_tex = 0;
        auto* rt = m_context.runtime();
        if (rt) {
            grid_tex = rt->get_sim_texture(entity);
        }

        if (grid_tex == 0) {
            grid_tex = get_pixel_grid_texture(entity, grid_comp.pixel_grid_path);
        }

        if (grid_tex != 0) {
            uint8_t tr = static_cast<uint8_t>(renderer.tint_r * renderer.opacity * 255.0f);
            uint8_t tg = static_cast<uint8_t>(renderer.tint_g * renderer.opacity * 255.0f);
            uint8_t tb = static_cast<uint8_t>(renderer.tint_b * renderer.opacity * 255.0f);
            uint8_t ta = static_cast<uint8_t>(renderer.tint_a * renderer.opacity * 255.0f);
            ImU32 tint = IM_COL32(tr, tg, tb, ta);

            draw_list->AddImageQuad(
                (ImTextureID)(uintptr_t)grid_tex,
                p0, p1, p2, p3,
                ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                tint
            );
        }
        // No red outline fallback in game view — just skip missing textures
    }

    draw_list->PopClipRect();

    cleanup_texture_cache();
}

GLuint GamePanel::get_pixel_grid_texture(entt::entity entity, const std::string& path) {
    if (path.empty()) return 0;

    auto it = m_grid_textures.find(entity);
    if (it != m_grid_textures.end()) {
        if (it->second.source_path == path) {
            return it->second.texture_id;
        }
        if (it->second.texture_id) {
            glDeleteTextures(1, &it->second.texture_id);
        }
        m_grid_textures.erase(it);
    }

    auto pxg_file = engine::asset::pxg_load(path);
    if (!pxg_file) return 0;

    auto parsed = engine::asset::parse_pxg(*pxg_file);
    if (parsed.width <= 0 || parsed.height <= 0) return 0;

    std::vector<uint8_t> rgba;

    if (parsed.has_color_layer && !parsed.color_rgba.empty()) {
        rgba = std::move(parsed.color_rgba);
    } else {
        auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
        std::vector<uint32_t> palette(256, 0x00000000);
        if (lib) {
            palette = lib->build_color_palette();
        }

        int pixel_count = parsed.width * parsed.height;
        rgba.resize(pixel_count * 4);
        for (int i = 0; i < pixel_count; i++) {
            uint8_t mat_id = parsed.material_ids.empty() ? 0 : parsed.material_ids[i];
            if (mat_id == 0) {
                rgba[i * 4 + 0] = 0;
                rgba[i * 4 + 1] = 0;
                rgba[i * 4 + 2] = 0;
                rgba[i * 4 + 3] = 0;
            } else {
                uint32_t color = palette[mat_id];
                rgba[i * 4 + 0] = (color >> 24) & 0xFF;
                rgba[i * 4 + 1] = (color >> 16) & 0xFF;
                rgba[i * 4 + 2] = (color >> 8)  & 0xFF;
                rgba[i * 4 + 3] = (color >> 0)  & 0xFF;
            }
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, parsed.width, parsed.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_grid_textures[entity] = CachedGridTexture{tex, path, parsed.width, parsed.height};
    return tex;
}

void GamePanel::cleanup_texture_cache() {
    auto* registry = m_context.registry();
    if (!registry) return;

    std::vector<entt::entity> to_remove;
    for (auto& [entity, cached] : m_grid_textures) {
        if (!registry->valid(entity) ||
            !registry->all_of<engine::simulation::PixelGridComponent,
                              engine::render::PixelGridRenderer>(entity)) {
            to_remove.push_back(entity);
        }
    }

    for (auto entity : to_remove) {
        auto& cached = m_grid_textures[entity];
        if (cached.texture_id) {
            glDeleteTextures(1, &cached.texture_id);
        }
        m_grid_textures.erase(entity);
    }
}

} // namespace editor

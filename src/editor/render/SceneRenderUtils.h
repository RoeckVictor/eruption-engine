#pragma once

#include "engine/core/MathConstants.h"
#include "engine/core/Transform.h"
#include "engine/core/ScreenRect.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"
#include "editor/core/PixelGridTextureCache.h"
#include "editor/core/RuntimeContext.h"
#include "editor/core/EditorComponents.h"

#include <imgui.h>
#include <entt/entt.hpp>
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

namespace editor {

struct WorldToScreen {
    float screen_cx;
    float screen_cy;
    float cam_x;
    float cam_y;
    float zoom;

    WorldToScreen(ImVec2 viewport_pos, ImVec2 viewport_size, float cx, float cy, float z)
        : screen_cx(viewport_pos.x + viewport_size.x * 0.5f)
        , screen_cy(viewport_pos.y + viewport_size.y * 0.5f)
        , cam_x(cx), cam_y(cy), zoom(z)
    {}

    ImVec2 operator()(float wx, float wy) const {
        float sx = screen_cx + (wx - cam_x) * zoom;
        float sy = screen_cy - (wy - cam_y) * zoom;
        return ImVec2(sx, sy);
    }
};

struct PixelGridQuad {
    ImVec2 corners[4];
    ImU32 tint;
    bool valid = false;
};

inline PixelGridQuad compute_pixel_grid_quad(
    const engine::Transform& transform,
    const engine::simulation::PixelGridComponent& grid_comp,
    const engine::render::PixelGridRenderer& renderer,
    const WorldToScreen& wts)
{
    PixelGridQuad quad;

    float w = grid_comp.width > 0 ? static_cast<float>(grid_comp.width) : 32.0f;
    float h = grid_comp.height > 0 ? static_cast<float>(grid_comp.height) : 32.0f;
    float ox = static_cast<float>(grid_comp.origin_x);
    float oy = static_cast<float>(grid_comp.origin_y);
    float sx = transform.world_scale_x;
    float sy = transform.world_scale_y;
    float rot_rad = transform.world_rotation * engine::DEG_TO_RAD;
    float cos_r = std::cos(rot_rad);
    float sin_r = std::sin(rot_rad);

    // 4 corners in local space (relative to origin, scaled)
    float lx[4] = {-ox * sx,      (w - ox) * sx, (w - ox) * sx, -ox * sx};
    float ly[4] = {(h - oy) * sy, (h - oy) * sy, -oy * sy,      -oy * sy};

    // Rotate around world position and convert to screen
    for (int i = 0; i < 4; ++i) {
        float wx = transform.world_x + lx[i] * cos_r - ly[i] * sin_r;
        float wy = transform.world_y + lx[i] * sin_r + ly[i] * cos_r;
        quad.corners[i] = wts(wx, wy);
    }

    // Compute tint color from renderer properties
    uint8_t tr = static_cast<uint8_t>(renderer.tint_r * renderer.opacity * 255.0f);
    uint8_t tg = static_cast<uint8_t>(renderer.tint_g * renderer.opacity * 255.0f);
    uint8_t tb = static_cast<uint8_t>(renderer.tint_b * renderer.opacity * 255.0f);
    uint8_t ta = static_cast<uint8_t>(renderer.tint_a * renderer.opacity * 255.0f);
    quad.tint = IM_COL32(tr, tg, tb, ta);
    quad.valid = true;

    return quad;
}

inline void draw_pixel_grid_quad(ImDrawList* draw_list, const PixelGridQuad& quad, void* texture_handle) {
    if (texture_handle != nullptr) {
        draw_list->AddImageQuad(
            (ImTextureID)(uintptr_t)texture_handle,
            quad.corners[0], quad.corners[1], quad.corners[2], quad.corners[3],
            ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
            quad.tint
        );
    } else {
        // Missing/unloaded grid: draw a red outline
        draw_list->AddQuad(
            quad.corners[0], quad.corners[1], quad.corners[2], quad.corners[3],
            IM_COL32(200, 80, 80, 180), 1.5f
        );
    }
}

inline void draw_selection_outline(ImDrawList* draw_list, const PixelGridQuad& quad) {
    draw_list->AddQuad(
        quad.corners[0], quad.corners[1], quad.corners[2], quad.corners[3],
        IM_COL32(255, 200, 50, 220), 2.0f
    );
}

// Resolve the texture handle for a pixel grid entity.
// Prefers the live simulation texture (during play mode), falling back to the
// cached static texture loaded from the .pxg file on disk.
inline void* resolve_grid_texture(entt::entity entity,
                                   const std::string& pxg_path,
                                   RuntimeContext* runtime,
                                   PixelGridTextureCache& cache) {
    void* tex = runtime ? runtime->get_sim_texture(entity) : nullptr;
    return tex ? tex : cache.get(entity, pxg_path);
}

inline ImU32 compute_image_tint(const engine::render::Image& image) {
    return IM_COL32(
        static_cast<uint8_t>(image.color_r * 255.0f),
        static_cast<uint8_t>(image.color_g * 255.0f),
        static_cast<uint8_t>(image.color_b * 255.0f),
        static_cast<uint8_t>(image.color_a * 255.0f)
    );
}

struct ImageUV {
    float u0, v0;
    float u1, v1;
};

inline ImageUV compute_image_uv(const engine::render::Image& image) {
    return {
        image.flip_x ? image.uv_max_x : image.uv_min_x,
        image.flip_y ? image.uv_max_y : image.uv_min_y,
        image.flip_x ? image.uv_min_x : image.uv_max_x,
        image.flip_y ? image.uv_min_y : image.uv_max_y
    };
}

struct ImageQuad {
    ImVec2 corners[4];
    ImageUV uv;
    ImU32 tint;
};

inline ImageQuad compute_image_quad(
    const engine::Transform& transform,
    int tex_w, int tex_h,
    const engine::render::Image& image,
    const WorldToScreen& wts)
{
    ImageQuad quad;

    float w = static_cast<float>(tex_w) * transform.world_scale_x;
    float h = static_cast<float>(tex_h) * transform.world_scale_y;
    float hw = w * 0.5f;
    float hh = h * 0.5f;

    float rot_rad = transform.world_rotation * engine::DEG_TO_RAD;
    float cos_r = std::cos(rot_rad);
    float sin_r = std::sin(rot_rad);

    // Local corners (centered on transform origin)
    // Order: TL, TR, BR, BL
    float lx[4] = { -hw, hw, hw, -hw };
    float ly[4] = { hh, hh, -hh, -hh };

    for (int i = 0; i < 4; ++i) {
        float wx = transform.world_x + lx[i] * cos_r - ly[i] * sin_r;
        float wy = transform.world_y + lx[i] * sin_r + ly[i] * cos_r;
        quad.corners[i] = wts(wx, wy);
    }

    quad.uv = compute_image_uv(image);
    quad.tint = compute_image_tint(image);

    return quad;
}

inline void draw_image_quad(ImDrawList* draw_list, const ImageQuad& quad, void* texture_handle) {
    draw_list->AddImageQuad(
        (ImTextureID)(uintptr_t)texture_handle,
        quad.corners[0], quad.corners[1], quad.corners[2], quad.corners[3],
        ImVec2(quad.uv.u0, quad.uv.v0),
        ImVec2(quad.uv.u1, quad.uv.v0),
        ImVec2(quad.uv.u1, quad.uv.v1),
        ImVec2(quad.uv.u0, quad.uv.v1),
        quad.tint
    );
}

enum class RenderableType {
    PixelGrid,
    Image,
    Text
};

struct RenderableItem {
    entt::entity entity;
    int layer;
    RenderableType type;
};

inline std::vector<RenderableItem> collect_world_renderables(entt::registry& registry) {
    std::vector<RenderableItem> items;

    // Collect PixelGrid entities
    {
        auto view = registry.view<engine::Transform,
                                   engine::simulation::PixelGridComponent,
                                   engine::render::PixelGridRenderer>();
        for (auto entity : view) {
            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }
            auto& renderer = view.get<engine::render::PixelGridRenderer>(entity);
            if (!renderer.enabled) continue;
            items.push_back({entity, renderer.layer, RenderableType::PixelGrid});
        }
    }

    // Collect world-space Image entities (Transform + Image, no ScreenRect)
    {
        auto view = registry.view<engine::Transform, engine::render::Image>();
        for (auto entity : view) {
            if (registry.all_of<engine::ScreenRect>(entity)) continue;  // Skip screen-space
            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }
            auto& image = view.get<engine::render::Image>(entity);
            if (!image.enabled) continue;
            items.push_back({entity, image.layer, RenderableType::Image});
        }
    }

    // Collect world-space Text entities (Transform + Text, no ScreenRect)
    {
        auto view = registry.view<engine::Transform, engine::render::Text>();
        for (auto entity : view) {
            if (registry.all_of<engine::ScreenRect>(entity)) continue;  // Skip screen-space
            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }
            auto& text = view.get<engine::render::Text>(entity);
            if (!text.enabled) continue;
            items.push_back({entity, text.layer, RenderableType::Text});
        }
    }

    // Sort by layer (ascending)
    std::sort(items.begin(), items.end(),
              [](const RenderableItem& a, const RenderableItem& b) {
                  return a.layer < b.layer;
              });

    return items;
}

}

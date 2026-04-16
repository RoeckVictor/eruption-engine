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
#include "editor/core/CoordinateUtils.h"
#include "editor/core/EditorPixelGridLoader.h"
#include "editor/render/EditorTextureCache.h"
#include "editor/render/EditorTextRenderer.h"

#include <imgui.h>
#include <entt/entt.hpp>
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

namespace editor {

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

inline void* resolve_grid_texture(entt::entity entity,
                                   const std::string& pxg_path,
                                   RuntimeContext* runtime,
                                   PixelGridTextureCache& cache,
                                   EditorPixelGridLoader* loader = nullptr) {
    // First, check for live simulation texture (SimSurface during play mode)
    void* tex = runtime ? runtime->get_sim_texture(entity) : nullptr;
    if (tex) return tex;

    // Check if loader has dirty pixel data that needs texture update
    if (loader && loader->is_dirty(entity)) {
        const auto* grid = loader->get_loaded_grid(entity);
        if (grid && grid->width > 0 && grid->height > 0) {
            void* updated_tex = nullptr;

            if (grid->has_color_layer && !grid->color_rgba.empty()) {
                updated_tex = cache.update_from_data(entity, grid->width, grid->height, grid->color_rgba);
            } else if (grid->has_material_layer && !grid->material_ids.empty()) {
                updated_tex = cache.update_from_materials(entity, grid->width, grid->height, grid->material_ids);
            }

            if (updated_tex) {
                loader->clear_dirty(entity);
                return updated_tex;
            }
        }
    }

    if (loader && pxg_path.empty()) {
        const auto* grid = loader->get_loaded_grid(entity);
        if (grid && grid->width > 0 && grid->height > 0) {
            // Check if we already have a cached texture for this entity
            void* cached = cache.get(entity, "");
            if (cached) return cached;

            // First time seeing this fragment - create texture
            void* updated_tex = nullptr;
            if (grid->has_color_layer && !grid->color_rgba.empty()) {
                updated_tex = cache.update_from_data(entity, grid->width, grid->height, grid->color_rgba);
            } else if (grid->has_material_layer && !grid->material_ids.empty()) {
                updated_tex = cache.update_from_materials(entity, grid->width, grid->height, grid->material_ids);
            }
            if (updated_tex) return updated_tex;
        }
    }

    // Fall back to cached static texture from .pxg file
    return cache.get(entity, pxg_path);
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

// ─────────────────────────────────────────────────────────────────────────────
// Screen-space entity collection (ScreenRect-based UI entities)
// ─────────────────────────────────────────────────────────────────────────────

struct ScreenRenderEntry {
    entt::entity entity;
    int layer;
    bool has_image;
    bool has_text;
};

// Collect all visible ScreenRect entities, sorted by layer.
// Set skip_empty to true to exclude entities that have neither Image nor Text.
inline std::vector<ScreenRenderEntry> collect_screen_renderables(
    entt::registry& registry, bool skip_empty = false)
{
    std::vector<ScreenRenderEntry> entries;

    auto view = registry.view<engine::ScreenRect>();
    for (auto entity : view) {
        auto& rect = view.get<engine::ScreenRect>(entity);

        if (registry.all_of<EntityInfo>(entity)) {
            if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
        }
        if (!rect.enabled) continue;

        bool has_image = registry.all_of<engine::render::Image>(entity);
        bool has_text  = registry.all_of<engine::render::Text>(entity);

        if (skip_empty && !has_image && !has_text) continue;

        int layer = 0;
        if (has_image) {
            layer = registry.get<engine::render::Image>(entity).layer;
        } else if (has_text) {
            layer = registry.get<engine::render::Text>(entity).layer;
        }

        entries.push_back({ entity, layer, has_image, has_text });
    }

    std::sort(entries.begin(), entries.end(),
              [](const ScreenRenderEntry& a, const ScreenRenderEntry& b) {
                  return a.layer < b.layer;
              });

    return entries;
}

// Render a screen-space Image entity using a ScreenCanvasTransform.
inline void render_screen_image(ImDrawList* draw_list,
                                entt::registry& registry,
                                EditorTextureCache& tex_cache,
                                const ScreenCanvasTransform& sct,
                                entt::entity entity,
                                ImVec2 canvas_pos, ImVec2 canvas_size)
{
    auto& rect  = registry.get<engine::ScreenRect>(entity);
    auto& image = registry.get<engine::render::Image>(entity);
    if (!image.enabled) return;

    int tex_w, tex_h;
    void* texture = tex_cache.get(image.sprite_path, tex_w, tex_h);

    ImVec2 tl = sct.to_canvas(rect.computed_x, rect.computed_y, canvas_pos, canvas_size);
    ImVec2 br = sct.to_canvas(rect.computed_x + rect.computed_width,
                              rect.computed_y + rect.computed_height,
                              canvas_pos, canvas_size);

    auto uv   = compute_image_uv(image);
    ImU32 tint = compute_image_tint(image);

    draw_list->AddImage(
        (ImTextureID)(uintptr_t)texture,
        tl, br,
        ImVec2(uv.u0, uv.v0), ImVec2(uv.u1, uv.v1),
        tint
    );
}

// Render a screen-space Text entity using a ScreenCanvasTransform.
inline void render_screen_text(ImDrawList* draw_list,
                               entt::registry& registry,
                               EditorTextRenderer* text_renderer,
                               const ScreenCanvasTransform& sct,
                               entt::entity entity,
                               ImVec2 canvas_pos, ImVec2 canvas_size)
{
    auto& rect = registry.get<engine::ScreenRect>(entity);
    auto& text = registry.get<engine::render::Text>(entity);
    if (!text.enabled) return;

    ImVec2 area_pos  = sct.to_canvas(rect.computed_x, rect.computed_y, canvas_pos, canvas_size);
    ImVec2 area_size(rect.width * sct.zoom, rect.height * sct.zoom);

    if (text_renderer) {
        text_renderer->render_in_area(draw_list, text, area_pos, area_size, sct.zoom);
    } else {
        ImU32 color = IM_COL32(
            static_cast<uint8_t>(text.color_r * 255.0f),
            static_cast<uint8_t>(text.color_g * 255.0f),
            static_cast<uint8_t>(text.color_b * 255.0f),
            static_cast<uint8_t>(text.color_a * 255.0f)
        );
        draw_list->AddText(area_pos, color, text.content.c_str());
    }
}

}

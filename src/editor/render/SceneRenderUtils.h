#pragma once

#include "engine/core/MathConstants.h"
#include "engine/core/Transform.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/render/PixelGridRenderer.h"
#include "editor/core/PixelGridTextureCache.h"
#include "editor/core/RuntimeContext.h"

#include <imgui.h>
#include <glad/gl.h>
#include <cstdint>
#include <cmath>

namespace editor {

/// Converts world coordinates to screen coordinates given a camera and viewport.
struct WorldToScreen {
    float screen_cx;  // Viewport center X in screen space
    float screen_cy;  // Viewport center Y in screen space
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
        float sy = screen_cy - (wy - cam_y) * zoom;  // Flip Y for screen space
        return ImVec2(sx, sy);
    }
};

/// Computed screen-space quad for a pixel grid entity.
struct PixelGridQuad {
    ImVec2 corners[4];  // top-left, top-right, bottom-right, bottom-left
    ImU32 tint;         // Combined tint + opacity color
    bool valid = false;
};

/// Compute the screen-space quad corners and tint for a pixel grid entity.
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

/// Draw a pixel grid quad with a texture to an ImGui draw list.
inline void draw_pixel_grid_quad(ImDrawList* draw_list, const PixelGridQuad& quad, GLuint texture_id) {
    if (texture_id != 0) {
        draw_list->AddImageQuad(
            (ImTextureID)(uintptr_t)texture_id,
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

/// Draw a selection outline around a pixel grid quad.
inline void draw_selection_outline(ImDrawList* draw_list, const PixelGridQuad& quad) {
    draw_list->AddQuad(
        quad.corners[0], quad.corners[1], quad.corners[2], quad.corners[3],
        IM_COL32(255, 200, 50, 220), 2.0f
    );
}

/// Resolve the GL texture for a pixel grid entity.
/// Prefers the live simulation texture (during play mode), falling back to the
/// cached static texture loaded from the .pxg file on disk.
inline GLuint resolve_grid_texture(entt::entity entity,
                                    const std::string& pxg_path,
                                    RuntimeContext* runtime,
                                    PixelGridTextureCache& cache) {
    GLuint tex = runtime ? runtime->get_sim_texture(entity) : 0;
    return tex ? tex : cache.get(entity, pxg_path);
}

} // namespace editor

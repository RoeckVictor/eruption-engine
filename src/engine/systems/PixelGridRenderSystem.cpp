#include "PixelGridRenderSystem.h"
#include "PixelGridLoaderSystem.h"
#include "engine/core/Engine.h"
#include "engine/core/Transform.h"
#include "engine/core/Logger.h"
#include "engine/render/Camera2D.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/graphics/Texture.h"
#include "engine/core/EngineContext.h"
#include "engine/simulation/MaterialLibrary.h"
#include "editor/core/EditorComponents.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace engine {

bool PixelGridRenderSystem::init(Engine& engine) {
    auto& ctx = engine.app_context<EngineContext>();
    m_registry = &ctx.registry;
    m_camera = &ctx.camera;

    // Load shaders
    if (!m_sprite_shader.load_graphics("shaders/textured_sprite.vert",
                                        "shaders/pixel_grid_sprite.frag")) {
        return false;
    }

    // Create quad VAO/VBO for sprite rendering
    // Vertices: position (x, y), UV (u, v), color (r, g, b, a)
    float quad_vertices[] = {
        // pos      // uv       // color
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Bottom-left
        1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Bottom-right
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Top-right

        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Bottom-left
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Top-right
        0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f   // Top-left
    };

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);

    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    // UV attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));

    // Color attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));

    glBindVertexArray(0);

    // Cache material library pointer for legacy palette fallback
    m_material_lib = simulation::MaterialLibraryRegistry::instance().get_library("default");

    Logger::instance().info("PixelGridRender", "PixelGridRenderSystem initialized");
    return true;
}

void PixelGridRenderSystem::shutdown() {
    m_sprite_shader.destroy();

    if (m_quad_vao) {
        glDeleteVertexArrays(1, &m_quad_vao);
        m_quad_vao = 0;
    }
    if (m_quad_vbo) {
        glDeleteBuffers(1, &m_quad_vbo);
        m_quad_vbo = 0;
    }

    // Clean up cached textures
    for (auto& [entity, texture] : m_cached_textures) {
        texture.destroy();
    }
    m_cached_textures.clear();
}

void PixelGridRenderSystem::ensure_texture_for_entity(entt::entity entity, const simulation::PixelGridComponent& grid_comp) {
    // Check if texture already exists
    if (m_cached_textures.find(entity) != m_cached_textures.end()) {
        return;
    }

    // Get loaded pixel grid data from loader system if available
    if (!m_loader) {
        static bool warned = false;
        if (!warned) {
            Logger::instance().warning("PixelGridRender", "Loader not set - call set_loader() before rendering");
            warned = true;
        }
        return;
    }

    const LoadedPixelGrid* grid_data = m_loader->get_loaded_grid(entity);
    if (!grid_data) {
        return;
    }

    graphics::Texture texture;

    if (grid_data->has_color_layer && !grid_data->color_rgba.empty()) {
        // New format: direct RGBA color from the .pxg color layer
        if (!texture.create_2d(grid_data->width, grid_data->height,
                               graphics::TextureFormat::RGBA8,
                               graphics::TextureFilter::Nearest,
                               graphics::TextureWrap::ClampToEdge,
                               grid_data->color_rgba.data())) {
            return;
        }
    } else {
        // Legacy fallback: build RGBA from material palette
        std::vector<uint32_t> palette(256, 0);
        if (m_material_lib) {
            palette = m_material_lib->build_color_palette();
        }

        int pixel_count = grid_data->width * grid_data->height;
        std::vector<uint8_t> rgba(pixel_count * 4);
        for (int i = 0; i < pixel_count; i++) {
            uint8_t mat_id = grid_data->material_ids.empty() ? 0 : grid_data->material_ids[i];
            if (mat_id == 0) {
                rgba[i * 4 + 0] = 0;
                rgba[i * 4 + 1] = 0;
                rgba[i * 4 + 2] = 0;
                rgba[i * 4 + 3] = 0;
            } else {
                uint32_t c = palette[mat_id];
                rgba[i * 4 + 0] = (c >> 24) & 0xFF;
                rgba[i * 4 + 1] = (c >> 16) & 0xFF;
                rgba[i * 4 + 2] = (c >> 8)  & 0xFF;
                rgba[i * 4 + 3] = c & 0xFF;
            }
        }

        if (!texture.create_2d(grid_data->width, grid_data->height,
                               graphics::TextureFormat::RGBA8,
                               graphics::TextureFilter::Nearest,
                               graphics::TextureWrap::ClampToEdge,
                               rgba.data())) {
            return;
        }
    }

    m_cached_textures[entity] = std::move(texture);
    Logger::instance().info("PixelGridRender", "Created texture for entity (%dx%d, color=%s)",
                            grid_data->width, grid_data->height,
                            grid_data->has_color_layer ? "direct" : "palette");
}

void PixelGridRenderSystem::purge_stale_textures() {
    for (auto it = m_cached_textures.begin(); it != m_cached_textures.end(); ) {
        if (!m_registry || !m_registry->valid(it->first)) {
            it->second.destroy();
            it = m_cached_textures.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_texture_overrides.begin(); it != m_texture_overrides.end(); ) {
        if (!m_registry || !m_registry->valid(it->first)) {
            it = m_texture_overrides.erase(it);
        } else {
            ++it;
        }
    }
}

void PixelGridRenderSystem::render(Engine& engine) {
    auto& window = engine.window();

    // Clean up textures for destroyed entities
    purge_stale_textures();

    // Collect all renderable entities
    struct RenderItem {
        entt::entity entity;
        int layer;
        const Transform* transform;
        const render::PixelGridRenderer* renderer;
        const simulation::PixelGridComponent* grid_comp;
    };

    std::vector<RenderItem> items;
    auto view = m_registry->view<Transform, render::PixelGridRenderer, simulation::PixelGridComponent>();

    for (auto entity : view) {
        // Skip disabled entities
        if (m_registry->all_of<editor::EntityInfo>(entity)) {
            if (!m_registry->get<editor::EntityInfo>(entity).enabled_in_hierarchy) {
                continue;
            }
        }

        auto& transform = view.get<Transform>(entity);
        auto& renderer = view.get<render::PixelGridRenderer>(entity);
        auto& grid_comp = view.get<simulation::PixelGridComponent>(entity);

        // Skip if renderer or grid disabled or not loaded
        if (!renderer.enabled || !grid_comp.enabled || !grid_comp.loaded) {
            continue;
        }

        // Ensure texture exists
        ensure_texture_for_entity(entity, grid_comp);

        items.push_back({entity, renderer.layer, &transform, &renderer, &grid_comp});
    }

    // Sort by layer (ascending)
    std::sort(items.begin(), items.end(), [](const RenderItem& a, const RenderItem& b) {
        return a.layer < b.layer;
    });

    // Render each item
    m_sprite_shader.use();

    // Set camera uniforms (constant for all sprites)
    m_sprite_shader.set_vec2("u_camera_pos", m_camera->x, m_camera->y);
    m_sprite_shader.set_vec2("u_screen_size", static_cast<float>(window.width()), static_cast<float>(window.height()));
    m_sprite_shader.set_float("u_zoom", m_camera->zoom);

    // Set texture unit
    m_sprite_shader.set_int("u_grid", 0);

    glBindVertexArray(m_quad_vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& item : items) {
        // Check for texture override (e.g. live simulation texture)
        auto override_it = m_texture_overrides.find(item.entity);
        if (override_it != m_texture_overrides.end()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, override_it->second);
        } else {
            // Get cached texture from loader
            auto it = m_cached_textures.find(item.entity);
            if (it == m_cached_textures.end()) {
                continue;
            }
            it->second.bind(0);
        }

        // Set per-sprite uniforms
        m_sprite_shader.set_float("u_opacity", item.renderer->opacity);
        m_sprite_shader.set_vec4("u_tint",
                    item.renderer->tint_r, item.renderer->tint_g,
                    item.renderer->tint_b, item.renderer->tint_a);

        // Compute world-space quad corners with origin, scale, and rotation
        constexpr float DEFAULT_GRID_SIZE = 32.0f;
        constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
        float w = item.grid_comp->width > 0 ? static_cast<float>(item.grid_comp->width) : DEFAULT_GRID_SIZE;
        float h = item.grid_comp->height > 0 ? static_cast<float>(item.grid_comp->height) : DEFAULT_GRID_SIZE;
        float ox = static_cast<float>(item.grid_comp->origin_x);
        float oy = static_cast<float>(item.grid_comp->origin_y);
        float sx = item.transform->world_scale_x;
        float sy = item.transform->world_scale_y;
        float rot_rad = item.transform->world_rotation * DEG_TO_RAD;
        float cos_r = std::cos(rot_rad);
        float sin_r = std::sin(rot_rad);

        // Local-space corners (origin-adjusted, scaled)
        float lx0 = -ox * sx,       ly0 = (h - oy) * sy;
        float lx1 = (w - ox) * sx,  ly1 = (h - oy) * sy;
        float lx2 = (w - ox) * sx,  ly2 = -oy * sy;
        float lx3 = -ox * sx,       ly3 = -oy * sy;

        // Rotate and translate to world position
        auto to_world = [&](float lx, float ly, float& wx, float& wy) {
            wx = item.transform->world_x + lx * cos_r - ly * sin_r;
            wy = item.transform->world_y + lx * sin_r + ly * cos_r;
        };

        float p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y;
        to_world(lx0, ly0, p0x, p0y);  // top-left
        to_world(lx1, ly1, p1x, p1y);  // top-right
        to_world(lx2, ly2, p2x, p2y);  // bottom-right
        to_world(lx3, ly3, p3x, p3y);  // bottom-left

        float quad_vertices[] = {
            // pos          // uv       // color
            p0x, p0y,      0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            p1x, p1y,      1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            p2x, p2y,      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,

            p0x, p0y,      0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            p2x, p2y,      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            p3x, p3y,      0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        };

        glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad_vertices), quad_vertices);

        // Draw quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

} // namespace engine

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
#include <vector>

namespace engine {

bool PixelGridRenderSystem::init(Engine& engine) {
    auto& ctx = engine.app_context<EngineContext>();
    m_registry = &ctx.registry;
    m_camera = &ctx.camera;

    // Load shaders
    if (!m_sprite_shader.load_graphics("shaders/textured_sprite.vert",
                                        "shaders/pixel_grid_sprite.frag")) {
        Logger::instance().error("PixelGridRender", "Failed to load pixel grid sprite shaders");
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
        texture.create_2d(grid_data->width, grid_data->height,
                          graphics::TextureFormat::RGBA8,
                          graphics::TextureFilter::Nearest,
                          graphics::TextureWrap::ClampToEdge,
                          grid_data->color_rgba.data());
    } else {
        // Legacy fallback: build RGBA from material palette
        auto* lib = simulation::MaterialLibraryRegistry::instance().get_library("default");
        std::vector<uint32_t> palette(256, 0);
        if (lib) {
            palette = lib->build_color_palette();
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

        texture.create_2d(grid_data->width, grid_data->height,
                          graphics::TextureFormat::RGBA8,
                          graphics::TextureFilter::Nearest,
                          graphics::TextureWrap::ClampToEdge,
                          rgba.data());
    }

    m_cached_textures[entity] = std::move(texture);
    Logger::instance().info("PixelGridRender", "Created texture for entity (%dx%d, color=%s)",
                            grid_data->width, grid_data->height,
                            grid_data->has_color_layer ? "direct" : "palette");
}

void PixelGridRenderSystem::render(Engine& engine) {
    auto& window = engine.window();

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
        // Get cached texture
        auto it = m_cached_textures.find(item.entity);
        if (it == m_cached_textures.end()) {
            continue;
        }

        it->second.bind(0);

        // Set per-sprite uniforms
        m_sprite_shader.set_float("u_opacity", item.renderer->opacity);
        glUniform4f(glGetUniformLocation(m_sprite_shader.handle(), "u_tint"),
                    item.renderer->tint_r, item.renderer->tint_g,
                    item.renderer->tint_b, item.renderer->tint_a);

        // Update quad vertices to match entity position and size
        float x = item.transform->x;
        float y = item.transform->y;
        float w = static_cast<float>(item.grid_comp->width);
        float h = static_cast<float>(item.grid_comp->height);

        // Update VBO with transformed quad
        float quad_vertices[] = {
            // pos         // uv       // color
            x,     y,     0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Bottom-left
            x + w, y,     1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Bottom-right
            x + w, y + h, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Top-right

            x,     y,     0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Bottom-left
            x + w, y + h, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Top-right
            x,     y + h, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f   // Top-left
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

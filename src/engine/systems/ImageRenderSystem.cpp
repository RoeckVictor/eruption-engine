#include "ImageRenderSystem.h"
#include "engine/core/Engine.h"
#include "engine/core/Transform.h"
#include "engine/core/ScreenRect.h"
#include "engine/core/Logger.h"
#include "engine/core/EngineContext.h"
#include "engine/render/Camera2D.h"
#include "engine/render/Image.h"
#include "engine/asset/AssetDatabase.h"
#include "engine/asset/loaders/TextureLoader.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace engine {

bool ImageRenderSystem::init(Engine& engine) {
    auto& ctx = engine.app_context<EngineContext>();
    m_registry = &ctx.registry;
    m_camera = &ctx.camera;
    m_assets = &engine.assets();

    // Load shaders
    if (!m_shader.load_graphics("shaders/image.vert", "shaders/image.frag")) {
        Logger::instance().error("ImageRender", "Failed to load image shaders");
        return false;
    }

    // Create 1x1 white texture for solid color rendering
    if (!create_white_texture()) {
        Logger::instance().error("ImageRender", "Failed to create white texture");
        return false;
    }

    // Create quad VAO/VBO
    // Vertices: position (x, y), UV (u, v), color (r, g, b, a)
    float quad_vertices[] = {
        // pos      // uv       // color
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);

    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_DYNAMIC_DRAW);

    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    // UV attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));

    // Color attribute (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));

    glBindVertexArray(0);

    Logger::instance().info("ImageRender", "ImageRenderSystem initialized");
    return true;
}

void ImageRenderSystem::shutdown() {
    m_shader.destroy();
    m_white_texture.destroy();

    if (m_quad_vao) {
        glDeleteVertexArrays(1, &m_quad_vao);
        m_quad_vao = 0;
    }
    if (m_quad_vbo) {
        glDeleteBuffers(1, &m_quad_vbo);
        m_quad_vbo = 0;
    }

    m_texture_cache.clear();
}

bool ImageRenderSystem::create_white_texture() {
    // Create a 1x1 white pixel texture
    uint8_t white_pixel[4] = { 255, 255, 255, 255 };
    return m_white_texture.create_2d(1, 1,
                                     graphics::TextureFormat::RGBA8,
                                     graphics::TextureFilter::Nearest,
                                     graphics::TextureWrap::ClampToEdge,
                                     white_pixel);
}

/// Normalize a sprite path by stripping leading ./
/// Paths are kept as-is since they may refer to either:
/// - User project assets in Assets/ (capital A)
/// - Engine assets in assets/ (lowercase a)
static std::string normalize_sprite_path(const std::string& path) {
    std::string normalized = path;

    // Strip leading ./
    while (normalized.size() >= 2 && normalized[0] == '.' &&
           (normalized[1] == '/' || normalized[1] == '\\')) {
        normalized = normalized.substr(2);
    }

    return normalized;
}

graphics::Texture* ImageRenderSystem::get_texture(const std::string& sprite_path) {
    if (sprite_path.empty()) {
        return &m_white_texture;
    }

    // Normalize the path
    std::string normalized = normalize_sprite_path(sprite_path);

    // Check cache for existing handle
    auto it = m_texture_cache.find(normalized);
    if (it != m_texture_cache.end()) {
        // Always resolve through AssetDatabase to get current pointer
        if (auto* tex = m_assets->get(it->second)) {
            return tex;
        }
        // Handle became invalid (asset unloaded), remove from cache
        m_texture_cache.erase(it);
    }

    // Load via AssetDatabase
    auto handle = m_assets->load<graphics::Texture>(normalized);
    if (!handle.valid()) {
        return nullptr;
    }
    m_texture_cache[normalized] = handle;
    return m_assets->get(handle);
}

void ImageRenderSystem::render(Engine& engine) {
    auto& window = engine.window();

    // Render item struct for sorting
    struct RenderItem {
        entt::entity entity;
        int layer;
        bool is_screen_space;
        // World/screen position and size
        float x, y, w, h;
        float rotation;  // degrees (world space only)
        float scale_x, scale_y;
        // UV coordinates
        float u0, v0, u1, v1;
        // Color
        float r, g, b, a;
        // Texture
        graphics::Texture* texture;
    };

    std::vector<RenderItem> items;

    // Collect world-space entities (Transform + Image)
    {
        auto view = m_registry->view<Transform, render::Image>();
        for (auto entity : view) {
            auto& transform = view.get<Transform>(entity);
            auto& image = view.get<render::Image>(entity);

            if (!image.enabled) continue;

            auto* texture = get_texture(image.sprite_path);
            if (!texture) continue;

            // Determine size from texture if sprite_path is set
            float w = static_cast<float>(texture->width());
            float h = static_cast<float>(texture->height());

            // Apply UV coordinates (with flip support)
            float u0 = image.flip_x ? image.uv_max_x : image.uv_min_x;
            float v0 = image.flip_y ? image.uv_max_y : image.uv_min_y;
            float u1 = image.flip_x ? image.uv_min_x : image.uv_max_x;
            float v1 = image.flip_y ? image.uv_min_y : image.uv_max_y;

            items.push_back({
                entity,
                image.layer,
                false,  // is_screen_space
                transform.world_x, transform.world_y, w, h,
                transform.world_rotation,
                transform.world_scale_x, transform.world_scale_y,
                u0, v0, u1, v1,
                image.color_r, image.color_g, image.color_b, image.color_a,
                texture
            });
        }
    }

    // Collect screen-space entities (ScreenRect + Image)
    {
        auto view = m_registry->view<ScreenRect, render::Image>();
        for (auto entity : view) {
            // Skip if entity also has Transform (use world space instead)
            if (m_registry->all_of<Transform>(entity)) {
                continue;
            }

            auto& rect = view.get<ScreenRect>(entity);
            auto& image = view.get<render::Image>(entity);

            if (!rect.enabled || !image.enabled) continue;

            auto* texture = get_texture(image.sprite_path);
            if (!texture) continue;

            // Use pre-computed screen-space position from ScreenRectSystem
            float w = rect.computed_width;
            float h = rect.computed_height;
            float pos_x = rect.computed_x;
            float pos_y = rect.computed_y;

            // Apply UV coordinates (with flip support)
            float u0 = image.flip_x ? image.uv_max_x : image.uv_min_x;
            float v0 = image.flip_y ? image.uv_max_y : image.uv_min_y;
            float u1 = image.flip_x ? image.uv_min_x : image.uv_max_x;
            float v1 = image.flip_y ? image.uv_min_y : image.uv_max_y;

            items.push_back({
                entity,
                image.layer,
                true,  // is_screen_space
                pos_x, pos_y,
                w, h,
                0.0f,  // no rotation for screen space
                1.0f, 1.0f,  // no scale for screen space (already in computed size)
                u0, v0, u1, v1,
                image.color_r, image.color_g, image.color_b, image.color_a,
                texture
            });
        }
    }

    if (items.empty()) return;

    // Sort by layer (ascending)
    std::sort(items.begin(), items.end(), [](const RenderItem& a, const RenderItem& b) {
        return a.layer < b.layer;
    });

    // Begin rendering
    m_shader.use();

    // Set common uniforms
    m_shader.set_vec2("u_camera_pos", m_camera->x, m_camera->y);
    m_shader.set_vec2("u_screen_size",
                      static_cast<float>(window.width()),
                      static_cast<float>(window.height()));
    m_shader.set_float("u_zoom", m_camera->zoom);
    m_shader.set_int("u_texture", 0);

    glBindVertexArray(m_quad_vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

    for (const auto& item : items) {
        // Set screen space mode
        m_shader.set_bool("u_screen_space", item.is_screen_space);

        // Bind texture
        item.texture->bind(0);

        // Compute quad vertices
        float w = item.w * item.scale_x;
        float h = item.h * item.scale_y;

        float p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y;

        if (item.is_screen_space) {
            // Screen space: Y-down convention (matching ImGui/editor)
            // computed_x/y is top-left corner, (0,0) is top-left of screen
            p0x = item.x;           p0y = item.y;           // top-left
            p1x = item.x + item.w;  p1y = item.y;           // top-right
            p2x = item.x + item.w;  p2y = item.y + item.h;  // bottom-right
            p3x = item.x;           p3y = item.y + item.h;  // bottom-left
        } else {
            // World space: apply rotation around origin (image centered on transform)
            float cos_r = std::cos(item.rotation * DEG_TO_RAD);
            float sin_r = std::sin(item.rotation * DEG_TO_RAD);

            // Local-space corners (centered on transform origin)
            float hw = w * 0.5f;
            float hh = h * 0.5f;
            float lx0 = -hw, ly0 = hh;    // top-left
            float lx1 = hw,  ly1 = hh;    // top-right
            float lx2 = hw,  ly2 = -hh;   // bottom-right
            float lx3 = -hw, ly3 = -hh;   // bottom-left

            // Rotate and translate
            auto to_world = [&](float lx, float ly, float& wx, float& wy) {
                wx = item.x + lx * cos_r - ly * sin_r;
                wy = item.y + lx * sin_r + ly * cos_r;
            };

            to_world(lx0, ly0, p0x, p0y);
            to_world(lx1, ly1, p1x, p1y);
            to_world(lx2, ly2, p2x, p2y);
            to_world(lx3, ly3, p3x, p3y);
        }

        // Build vertex data with UVs and color
        float quad_vertices[] = {
            // pos          // uv              // color
            p0x, p0y,      item.u0, item.v0,  item.r, item.g, item.b, item.a,
            p1x, p1y,      item.u1, item.v0,  item.r, item.g, item.b, item.a,
            p2x, p2y,      item.u1, item.v1,  item.r, item.g, item.b, item.a,

            p0x, p0y,      item.u0, item.v0,  item.r, item.g, item.b, item.a,
            p2x, p2y,      item.u1, item.v1,  item.r, item.g, item.b, item.a,
            p3x, p3y,      item.u0, item.v1,  item.r, item.g, item.b, item.a,
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

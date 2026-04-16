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
#include "engine/profiler/Profiler.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"
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

    auto* device = rhi::get_current_device();
    if (!device) {
        Logger::instance().error("ImageRender", "No RHI device available");
        return false;
    }

    // Create quad VBO
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

    rhi::BufferDesc vbo_desc{};
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Dynamic;
    vbo_desc.size = sizeof(quad_vertices);
    vbo_desc.initial_data = quad_vertices;
    m_quad_vbo = device->create_buffer(vbo_desc);
    if (!m_quad_vbo || !m_quad_vbo->valid()) {
        Logger::instance().error("ImageRender", "Failed to create quad VBO");
        m_shader.destroy();
        return false;
    }

    // Create pipeline with vertex layout: position + UV + color
    rhi::VertexAttribute attrs[] = {
        { 0, rhi::VertexFormat::Float2, 0, 0 },   // Position
        { 1, rhi::VertexFormat::Float2, 8, 0 },   // UV
        { 2, rhi::VertexFormat::Float4, 16, 0 },  // Color
    };
    rhi::VertexBinding bindings[] = {
        { 0, 8 * sizeof(float), false },
    };

    rhi::PipelineDesc desc{};
    desc.shader = m_shader.rhi_shader();
    desc.topology = rhi::PrimitiveTopology::Triangles;
    desc.attributes = attrs;
    desc.attribute_count = 3;
    desc.bindings = bindings;
    desc.binding_count = 1;

    // Enable alpha blending
    desc.blend.enabled = true;
    desc.blend.src_color = rhi::BlendFactor::SrcAlpha;
    desc.blend.dst_color = rhi::BlendFactor::OneMinusSrcAlpha;
    desc.blend.src_alpha = rhi::BlendFactor::One;
    desc.blend.dst_alpha = rhi::BlendFactor::OneMinusSrcAlpha;

    // No culling for 2D sprites
    desc.rasterizer.cull_mode = rhi::CullMode::None;

    m_pipeline = device->create_pipeline(desc);
    if (!m_pipeline || !m_pipeline->valid()) {
        Logger::instance().error("ImageRender", "Failed to create pipeline");
        m_quad_vbo.reset();
        m_shader.destroy();
        return false;
    }

    Logger::instance().info("ImageRender", "ImageRenderSystem initialized");
    return true;
}

void ImageRenderSystem::shutdown() {
    m_pipeline.reset();
    m_quad_vbo.reset();
    m_shader.destroy();
    m_white_texture.destroy();
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
    PROFILE_SCOPE("ImageRenderSystem::render");
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
        // Clip bounds (if has_clip is true)
        bool has_clip = false;
        float clip_x = 0, clip_y = 0, clip_w = 0, clip_h = 0;
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

            // Check for clip bounds
            bool has_clip = false;
            float clip_x = 0, clip_y = 0, clip_w = 0, clip_h = 0;
            if (rect.clip_to != entt::null && m_registry->valid(rect.clip_to)) {
                ScreenRect* clip_rect = m_registry->try_get<ScreenRect>(rect.clip_to);
                if (clip_rect && clip_rect->enabled) {
                    has_clip = true;
                    clip_x = clip_rect->computed_x;
                    clip_y = clip_rect->computed_y;
                    clip_w = clip_rect->computed_width;
                    clip_h = clip_rect->computed_height;
                }
            }

            RenderItem item{};
            item.entity = entity;
            item.layer = image.layer;
            item.is_screen_space = true;
            item.x = pos_x;
            item.y = pos_y;
            item.w = w;
            item.h = h;
            item.rotation = 0.0f;
            item.scale_x = 1.0f;
            item.scale_y = 1.0f;
            item.u0 = u0;
            item.v0 = v0;
            item.u1 = u1;
            item.v1 = v1;
            item.r = image.color_r;
            item.g = image.color_g;
            item.b = image.color_b;
            item.a = image.color_a;
            item.texture = texture;
            item.has_clip = has_clip;
            item.clip_x = clip_x;
            item.clip_y = clip_y;
            item.clip_w = clip_w;
            item.clip_h = clip_h;
            items.push_back(item);
        }
    }

    if (items.empty()) return;

    // Sort by layer (ascending)
    std::sort(items.begin(), items.end(), [](const RenderItem& a, const RenderItem& b) {
        return a.layer < b.layer;
    });

    auto* ctx = rhi::get_current_context();
    if (!ctx) return;

    // Bind pipeline and set shader uniforms
    ctx->bind_pipeline(m_pipeline.get());

    // Set common uniforms
    m_shader.set_vec2("u_camera_pos", m_camera->x, m_camera->y);
    m_shader.set_vec2("u_screen_size",
                      static_cast<float>(window.width()),
                      static_cast<float>(window.height()));
    m_shader.set_float("u_zoom", m_camera->zoom);
    m_shader.set_int("u_texture", 0);

    // Bind vertex buffer
    ctx->bind_vertex_buffer(m_quad_vbo.get(), 0, 0);

    constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

    int screen_height = window.height();
    bool scissor_enabled = false;

    for (const auto& item : items) {
        // Handle scissor clipping via RHI abstraction
        if (item.has_clip) {
            if (!scissor_enabled) {
                ctx->enable_scissor_test(true);
                scissor_enabled = true;
            }
            // Convert from top-left origin to OpenGL bottom-left origin
            int scissor_y = screen_height - static_cast<int>(item.clip_y + item.clip_h);
            ctx->set_scissor(static_cast<int>(item.clip_x), scissor_y,
                             static_cast<int>(item.clip_w), static_cast<int>(item.clip_h));
        } else if (scissor_enabled) {
            ctx->enable_scissor_test(false);
            scissor_enabled = false;
        }

        // Set screen space mode
        m_shader.set_bool("u_screen_space", item.is_screen_space);

        // Bind texture
        ctx->bind_texture(item.texture->rhi_texture(), 0);

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

        m_quad_vbo->update(0, sizeof(quad_vertices), quad_vertices);

        // Draw quad
        ctx->draw(6, 0, 1);
    }

    // Disable scissor if it was enabled
    if (scissor_enabled) {
        ctx->enable_scissor_test(false);
    }
}

} // namespace engine

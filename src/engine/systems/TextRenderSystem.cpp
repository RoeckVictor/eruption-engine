#include "TextRenderSystem.h"
#include "engine/core/Engine.h"
#include "engine/core/Transform.h"
#include "engine/core/ScreenRect.h"
#include "engine/core/Logger.h"
#include "engine/core/EngineContext.h"
#include "engine/render/Camera2D.h"
#include "engine/asset/AssetDatabase.h"
#include "engine/asset/loaders/DynamicFontLoader.h"
#include "engine/profiler/Profiler.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"
#include <algorithm>
#include <cmath>

namespace engine {

bool TextRenderSystem::init(Engine& engine) {
    auto& ctx = engine.app_context<EngineContext>();
    m_registry = &ctx.registry;
    m_camera = &ctx.camera;
    m_assets = &engine.assets();

    // Load bitmap text shader
    if (!m_shader.load_graphics("shaders/bitmap_text.vert", "shaders/bitmap_text.frag")) {
        Logger::instance().error("TextRender", "Failed to load bitmap text shaders");
        return false;
    }

    auto* device = rhi::get_current_device();
    if (!device) {
        Logger::instance().error("TextRender", "No RHI device available");
        return false;
    }

    // Create initial VBO (will grow dynamically)
    constexpr size_t INITIAL_VBO_SIZE = 4096 * sizeof(TextVertex);
    rhi::BufferDesc vbo_desc{};
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Dynamic;
    vbo_desc.size = INITIAL_VBO_SIZE;
    vbo_desc.initial_data = nullptr;
    m_vbo = device->create_buffer(vbo_desc);
    if (!m_vbo || !m_vbo->valid()) {
        Logger::instance().error("TextRender", "Failed to create VBO");
        m_shader.destroy();
        return false;
    }
    m_vbo_capacity = INITIAL_VBO_SIZE;

    // Create pipeline with vertex layout: position + UV + color
    rhi::VertexAttribute attrs[] = {
        { 0, rhi::VertexFormat::Float2, static_cast<uint32_t>(offsetof(TextVertex, x)), 0 },   // Position
        { 1, rhi::VertexFormat::Float2, static_cast<uint32_t>(offsetof(TextVertex, u)), 0 },   // UV
        { 2, rhi::VertexFormat::Float4, static_cast<uint32_t>(offsetof(TextVertex, r)), 0 },   // Color
    };
    rhi::VertexBinding bindings[] = {
        { 0, sizeof(TextVertex), false },
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
        Logger::instance().error("TextRender", "Failed to create pipeline");
        m_vbo.reset();
        m_shader.destroy();
        return false;
    }

    Logger::instance().info("TextRender", "TextRenderSystem initialized");
    return true;
}

void TextRenderSystem::shutdown() {
    m_pipeline.reset();
    m_vbo.reset();
    m_shader.destroy();
    m_font_cache.clear();
}

render::DynamicFont* TextRenderSystem::get_font(const std::string& font_path) {
    std::string normalized = render::normalize_font_path(font_path);

    // Check cache for existing handle
    auto it = m_font_cache.find(normalized);
    if (it != m_font_cache.end()) {
        // Always resolve through AssetDatabase to get current pointer
        if (auto* font = m_assets->get(it->second)) {
            return font;
        }
        // Handle became invalid, remove from cache
        m_font_cache.erase(it);
    }

    // Load via AssetDatabase
    auto handle = m_assets->load<render::DynamicFont>(normalized);
    if (!handle.valid()) {
        return nullptr;
    }
    m_font_cache[normalized] = handle;
    return m_assets->get(handle);
}

void TextRenderSystem::layout_text(
    const render::Text& text,
    render::DynamicFont& font,
    float origin_x, float origin_y,
    float rotation,
    bool is_screen_space,
    std::vector<TextVertex>& out_vertices
) {
    auto codepoints = render::decode_utf8(text.content);
    if (codepoints.empty()) return;

    // Quantize font size for atlas lookup
    int atlas_size = render::quantize_font_size(text.font_size);
    auto* atlas = font.get_atlas(atlas_size);
    if (!atlas) return;

    // Scale factor if requested size differs from atlas size
    float render_scale = text.font_size / static_cast<float>(atlas_size);

    // Line height using font metrics
    float line_height_px = (atlas->ascent - atlas->descent + atlas->line_gap)
                           * text.line_height * render_scale;

    // Pre-calculate total text bounds for centering (world-space only)
    float total_width = 0.0f;
    int line_count = 1;
    {
        size_t line_start = 0;
        for (size_t i = 0; i <= codepoints.size(); ++i) {
            bool is_end = (i == codepoints.size());
            bool is_newline = !is_end && (codepoints[i] == '\n');
            if (is_newline || is_end) {
                float w = render::measure_line_width(codepoints, line_start, i, font, atlas_size, render_scale);
                total_width = std::max(total_width, w);
                if (is_newline) line_count++;
                line_start = i + 1;
            }
        }
    }
    float total_height = static_cast<float>(line_count) * line_height_px;

    // Rotation (world space only)
    constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
    float cos_r = is_screen_space ? 1.0f : std::cos(rotation * DEG_TO_RAD);
    float sin_r = is_screen_space ? 0.0f : std::sin(rotation * DEG_TO_RAD);

    // Adjust origin Y for baseline positioning
    float adjusted_origin_x = origin_x;
    float adjusted_origin_y = origin_y;
    if (is_screen_space) {
        adjusted_origin_y = origin_y + atlas->ascent * render_scale;
    } else {
        // Center text on transform position for world-space
        adjusted_origin_x = origin_x - total_width * 0.5f;
        adjusted_origin_y = origin_y + total_height * 0.5f;
    }

    auto to_world = [&](float lx, float ly, float& wx, float& wy) {
        wx = adjusted_origin_x + lx * cos_r - ly * sin_r;
        wy = adjusted_origin_y + lx * sin_r + ly * cos_r;
    };

    // Break text into lines
    struct Line {
        size_t start;
        size_t end;
        float width;
    };
    std::vector<Line> lines;

    size_t line_start = 0;
    for (size_t i = 0; i <= codepoints.size(); ++i) {
        bool is_end = (i == codepoints.size());
        bool is_newline = !is_end && (codepoints[i] == '\n');

        // Check for word wrap
        if (text.max_width > 0.0f && !is_end && !is_newline) {
            float line_width = render::measure_line_width(codepoints, line_start, i + 1, font, atlas_size, render_scale);
            if (line_width > text.max_width) {
                size_t break_at = i;
                for (size_t j = i; j > line_start; --j) {
                    if (codepoints[j - 1] == ' ') {
                        break_at = j;
                        break;
                    }
                }
                if (break_at > line_start) {
                    float w = render::measure_line_width(codepoints, line_start, break_at, font, atlas_size, render_scale);
                    lines.push_back({ line_start, break_at, w });
                    line_start = break_at;
                    if (line_start < codepoints.size() && codepoints[line_start] == ' ') {
                        line_start++;
                    }
                }
            }
        }

        if (is_newline || is_end) {
            if (i > line_start || is_end) {
                float w = render::measure_line_width(codepoints, line_start, i, font, atlas_size, render_scale);
                lines.push_back({ line_start, i, w });
            }
            line_start = i + 1;
        }
    }

    // Generate vertices for each line
    float cursor_y = 0.0f;

    for (const auto& line : lines) {
        // Calculate alignment offset
        float align_offset = 0.0f;
        switch (text.align) {
            case render::TextAlign::Center:
                align_offset = -line.width * 0.5f;
                break;
            case render::TextAlign::Right:
                align_offset = -line.width;
                break;
            default:
                break;
        }

        float cursor_x = align_offset;
        uint32_t prev_cp = 0;

        for (size_t i = line.start; i < line.end; ++i) {
            uint32_t cp = codepoints[i];
            if (cp == '\n' || cp == '\r') continue;

            const auto* glyph = font.get_glyph(atlas_size, cp);
            if (!glyph) continue;

            // Skip zero-width glyphs (like spaces with no visible content)
            if (glyph->atlas_w <= 0 || glyph->atlas_h <= 0) {
                if (prev_cp != 0) {
                    cursor_x += font.get_kerning(atlas_size, prev_cp, cp) * render_scale;
                }
                cursor_x += glyph->advance * render_scale;
                prev_cp = cp;
                continue;
            }

            // Apply kerning
            if (prev_cp != 0) {
                cursor_x += font.get_kerning(atlas_size, prev_cp, cp) * render_scale;
            }

            // Calculate quad position
            // bearing_x is offset from cursor to left edge
            // bearing_y is offset from baseline to top of glyph
            float x0 = cursor_x + glyph->bearing_x * render_scale;
            float x1 = x0 + glyph->atlas_w * render_scale;

            float glyph_y_top, glyph_y_bottom;
            if (is_screen_space) {
                // Y-down: baseline - bearing_y = top of glyph (smaller Y)
                glyph_y_top = cursor_y - glyph->bearing_y * render_scale;
                glyph_y_bottom = glyph_y_top + glyph->atlas_h * render_scale;
            } else {
                // Y-up: baseline + bearing_y = top of glyph (larger Y)
                glyph_y_top = cursor_y + glyph->bearing_y * render_scale;
                glyph_y_bottom = glyph_y_top - glyph->atlas_h * render_scale;
            }

            // Calculate UV coordinates
            float u0 = glyph->atlas_x / static_cast<float>(atlas->width);
            float v0 = glyph->atlas_y / static_cast<float>(atlas->height);
            float u1 = (glyph->atlas_x + glyph->atlas_w) / static_cast<float>(atlas->width);
            float v1 = (glyph->atlas_y + glyph->atlas_h) / static_cast<float>(atlas->height);

            // Transform to world/screen space
            float p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y;
            to_world(x0, glyph_y_top, p0x, p0y);
            to_world(x1, glyph_y_top, p1x, p1y);
            to_world(x1, glyph_y_bottom, p2x, p2y);
            to_world(x0, glyph_y_bottom, p3x, p3y);

            // Emit two triangles (top-left, top-right, bottom-right) and (top-left, bottom-right, bottom-left)
            out_vertices.push_back({ p0x, p0y, u0, v0, text.color_r, text.color_g, text.color_b, text.color_a });
            out_vertices.push_back({ p1x, p1y, u1, v0, text.color_r, text.color_g, text.color_b, text.color_a });
            out_vertices.push_back({ p2x, p2y, u1, v1, text.color_r, text.color_g, text.color_b, text.color_a });

            out_vertices.push_back({ p0x, p0y, u0, v0, text.color_r, text.color_g, text.color_b, text.color_a });
            out_vertices.push_back({ p2x, p2y, u1, v1, text.color_r, text.color_g, text.color_b, text.color_a });
            out_vertices.push_back({ p3x, p3y, u0, v1, text.color_r, text.color_g, text.color_b, text.color_a });

            cursor_x += glyph->advance * render_scale;
            prev_cp = cp;
        }

        // Move to next line
        if (is_screen_space) {
            cursor_y += line_height_px;
        } else {
            cursor_y -= line_height_px;
        }
    }
}

void TextRenderSystem::render(Engine& engine) {
    PROFILE_SCOPE("TextRenderSystem::render");
    auto& window = engine.window();

    // Collect text items for rendering
    struct TextItem {
        entt::entity entity;
        int layer;
        bool is_screen_space;
        float origin_x, origin_y;
        float rotation;
        const render::Text* text;
        render::DynamicFont* font;
        int font_size;
    };

    std::vector<TextItem> items;

    // Collect world-space entities (Transform + Text)
    {
        auto view = m_registry->view<Transform, render::Text>();
        for (auto entity : view) {
            auto& transform = view.get<Transform>(entity);
            auto& text = view.get<render::Text>(entity);

            if (!text.enabled) continue;

            std::string font_path = render::build_font_path(text.font_path, text.bold, text.italic);
            auto* font = get_font(font_path);
            if (!font) {
                font = get_font(text.font_path);
            }
            if (!font || !font->is_valid()) continue;

            int font_size = render::quantize_font_size(text.font_size);

            items.push_back({
                entity,
                text.layer,
                false,
                transform.world_x, transform.world_y,
                transform.world_rotation,
                &text,
                font,
                font_size
            });
        }
    }

    // Collect screen-space entities (ScreenRect + Text)
    {
        auto view = m_registry->view<ScreenRect, render::Text>();
        for (auto entity : view) {
            // Skip if entity also has Transform (use world space instead)
            if (m_registry->all_of<Transform>(entity)) {
                continue;
            }

            auto& rect = view.get<ScreenRect>(entity);
            auto& text = view.get<render::Text>(entity);

            if (!rect.enabled || !text.enabled) continue;

            std::string font_path = render::build_font_path(text.font_path, text.bold, text.italic);
            auto* font = get_font(font_path);
            if (!font) {
                font = get_font(text.font_path);
            }
            if (!font || !font->is_valid()) continue;

            int font_size = render::quantize_font_size(text.font_size);

            float pos_x = rect.computed_x;
            float pos_y = rect.computed_y;

            items.push_back({
                entity,
                text.layer,
                true,
                pos_x, pos_y,
                0.0f,
                &text,
                font,
                font_size
            });
        }
    }

    if (items.empty()) return;

    // Sort by layer
    std::sort(items.begin(), items.end(), [](const TextItem& a, const TextItem& b) {
        return a.layer < b.layer;
    });

    // Build all vertices
    std::vector<TextVertex> all_vertices;

    // Batch by font + size combination
    struct Batch {
        render::DynamicFont* font;
        int font_size;
        size_t start_vertex;
        size_t vertex_count;
        bool is_screen_space;
    };
    std::vector<Batch> batches;

    for (const auto& item : items) {
        size_t start = all_vertices.size();

        layout_text(*item.text, *item.font,
                    item.origin_x, item.origin_y,
                    item.rotation, item.is_screen_space,
                    all_vertices);

        size_t count = all_vertices.size() - start;
        if (count > 0) {
            batches.push_back({ item.font, item.font_size, start, count, item.is_screen_space });
        }
    }

    if (all_vertices.empty()) return;

    // Upload vertex data
    size_t data_size = all_vertices.size() * sizeof(TextVertex);
    if (data_size > m_vbo_capacity) {
        m_vbo_capacity = data_size * 2;
        m_vbo->resize(m_vbo_capacity, nullptr);
    }
    m_vbo->update(0, data_size, all_vertices.data());

    auto* ctx = rhi::get_current_context();
    if (!ctx) return;

    // Bind pipeline and set shader uniforms
    ctx->bind_pipeline(m_pipeline.get());

    m_shader.set_vec2("u_camera_pos", m_camera->x, m_camera->y);
    m_shader.set_vec2("u_screen_size",
                      static_cast<float>(window.width()),
                      static_cast<float>(window.height()));
    m_shader.set_float("u_zoom", m_camera->zoom);
    m_shader.set_int("u_texture", 0);

    // Bind vertex buffer
    ctx->bind_vertex_buffer(m_vbo.get(), 0, 0);

    for (const auto& batch : batches) {
        m_shader.set_bool("u_screen_space", batch.is_screen_space);

        auto* atlas = batch.font->get_atlas(batch.font_size);
        if (atlas) {
            ctx->bind_texture(atlas->texture.rhi_texture(), 0);
        }

        ctx->draw(static_cast<uint32_t>(batch.vertex_count),
                  static_cast<uint32_t>(batch.start_vertex), 1);
    }
}

} // namespace engine

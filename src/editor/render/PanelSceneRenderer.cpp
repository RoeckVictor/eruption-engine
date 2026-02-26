#include "PanelSceneRenderer.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/RuntimeContext.h"
#include "engine/core/Transform.h"
#include "engine/core/Engine.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/simulation/PixelGridComponent.h"

namespace editor {

PanelSceneRenderer::PanelSceneRenderer(EditorContext& context)
    : m_context(context)
{
}

PanelSceneRenderer::~PanelSceneRenderer() = default;

void PanelSceneRenderer::ensure_text_renderer() {
    if (m_text_renderer) return;

    auto* runtime = m_context.runtime();
    if (!runtime) return;

    auto* eng = runtime->engine();
    if (!eng) return;

    m_text_renderer = std::make_unique<EditorTextRenderer>(eng->assets());
}

void PanelSceneRenderer::render_world_image(ImDrawList* draw_list, entt::registry& registry,
                                             entt::entity entity, const WorldToScreen& wts) {
    if (!registry.valid(entity)) return;
    if (!registry.all_of<engine::Transform, engine::render::Image>(entity)) return;

    auto& transform = registry.get<engine::Transform>(entity);
    auto& image = registry.get<engine::render::Image>(entity);

    if (!image.enabled) return;

    int tex_width, tex_height;
    void* texture = m_image_textures.get(image.sprite_path, tex_width, tex_height);

    // Cache dimensions for hit detection
    image._cached_width = tex_width;
    image._cached_height = tex_height;

    auto quad = compute_image_quad(transform, tex_width, tex_height, image, wts);
    draw_image_quad(draw_list, quad, texture);
}

void PanelSceneRenderer::render_world_text(ImDrawList* draw_list, entt::registry& registry,
                                            entt::entity entity, const WorldToScreen& wts) {
    if (!registry.valid(entity)) return;
    if (!registry.all_of<engine::Transform, engine::render::Text>(entity)) return;

    ensure_text_renderer();
    if (!m_text_renderer) return;

    auto& transform = registry.get<engine::Transform>(entity);
    auto& text = registry.get<engine::render::Text>(entity);

    if (!text.enabled) return;

    ImVec2 center = wts(transform.world_x, transform.world_y);
    m_text_renderer->render_centered(draw_list, text, center, wts.zoom);
}

void PanelSceneRenderer::render_pixel_grid(ImDrawList* draw_list, entt::registry& registry,
                                            entt::entity entity, const WorldToScreen& wts,
                                            RuntimeContext* runtime) {
    if (!registry.valid(entity)) return;
    if (!registry.all_of<engine::Transform, engine::simulation::PixelGridComponent,
                         engine::render::PixelGridRenderer>(entity)) return;

    auto& transform = registry.get<engine::Transform>(entity);
    auto& grid_comp = registry.get<engine::simulation::PixelGridComponent>(entity);
    auto& renderer = registry.get<engine::render::PixelGridRenderer>(entity);

    auto quad = compute_pixel_grid_quad(transform, grid_comp, renderer, wts);
    void* grid_tex = resolve_grid_texture(entity, grid_comp.pixel_grid_path, runtime, m_grid_textures);

    draw_pixel_grid_quad(draw_list, quad, grid_tex);
}

void PanelSceneRenderer::render_world_entities(ImDrawList* draw_list, entt::registry& registry,
                                                const WorldToScreen& wts, bool draw_selection,
                                                RuntimeContext* runtime) {
    // Collect and sort all world-space renderables
    auto render_items = collect_world_renderables(registry);

    // Render sorted entities
    for (const auto& item : render_items) {
        switch (item.type) {
            case RenderableType::PixelGrid: {
                render_pixel_grid(draw_list, registry, item.entity, wts, runtime);

                if (draw_selection && m_context.selection().is_selected(item.entity)) {
                    auto& transform = registry.get<engine::Transform>(item.entity);
                    auto& grid_comp = registry.get<engine::simulation::PixelGridComponent>(item.entity);
                    auto& renderer = registry.get<engine::render::PixelGridRenderer>(item.entity);
                    auto quad = compute_pixel_grid_quad(transform, grid_comp, renderer, wts);
                    draw_selection_outline(draw_list, quad);
                }
                break;
            }
            case RenderableType::Image:
                render_world_image(draw_list, registry, item.entity, wts);
                break;
            case RenderableType::Text:
                render_world_text(draw_list, registry, item.entity, wts);
                break;
        }
    }
}

void PanelSceneRenderer::cleanup(entt::registry* registry) {
    m_grid_textures.cleanup(registry);
    // EditorTextureCache doesn't have entity-based cleanup, just path-based caching
}

void PanelSceneRenderer::clear_caches() {
    m_image_textures.clear();
    m_grid_textures.clear();
}

FramebufferResizeDebouncer::FramebufferResizeDebouncer(float debounce_sec)
    : m_debounce_sec(debounce_sec)
{
}

bool FramebufferResizeDebouncer::should_resize(int current_w, int current_h,
                                                int desired_w, int desired_h, float dt) {
    if (desired_w != m_pending_width || desired_h != m_pending_height) {
        m_pending_width = desired_w;
        m_pending_height = desired_h;
        m_timer = 0.0f;
        m_failed = false;
    }

    if (m_failed) {
        return false;
    }

    if (m_pending_width != current_w || m_pending_height != current_h) {
        m_timer += dt;

        if (m_timer >= m_debounce_sec || current_w == 0) {
            return true;
        }
    }

    return false;
}

}

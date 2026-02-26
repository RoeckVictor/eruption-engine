#pragma once

#include "EditorTextureCache.h"
#include "EditorTextRenderer.h"
#include "SceneRenderUtils.h"
#include "editor/core/PixelGridTextureCache.h"
#include <entt/entt.hpp>
#include <imgui.h>
#include <memory>

namespace editor {

class EditorContext;
class RuntimeContext;

// Consolidates scene rendering logic shared across multiple panels
// Handles world-space Image, Text, and PixelGrid rendering with proper
// texture caching and text rendering
class PanelSceneRenderer {
public:
    explicit PanelSceneRenderer(EditorContext& context);
    ~PanelSceneRenderer();

    void render_world_image(ImDrawList* draw_list, entt::registry& registry,
                            entt::entity entity, const WorldToScreen& wts);

    void render_world_text(ImDrawList* draw_list, entt::registry& registry,
                           entt::entity entity, const WorldToScreen& wts);

    void render_pixel_grid(ImDrawList* draw_list, entt::registry& registry,
                           entt::entity entity, const WorldToScreen& wts,
                           RuntimeContext* runtime = nullptr);

    void render_world_entities(ImDrawList* draw_list, entt::registry& registry,
                               const WorldToScreen& wts, bool draw_selection,
                               RuntimeContext* runtime = nullptr);

    void cleanup(entt::registry* registry);

    void clear_caches();

    EditorTextureCache& image_textures() { return m_image_textures; }
    const EditorTextureCache& image_textures() const { return m_image_textures; }

    PixelGridTextureCache& grid_textures() { return m_grid_textures; }
    const PixelGridTextureCache& grid_textures() const { return m_grid_textures; }

private:
    void ensure_text_renderer();

    EditorContext& m_context;
    EditorTextureCache m_image_textures;
    PixelGridTextureCache m_grid_textures;
    std::unique_ptr<EditorTextRenderer> m_text_renderer;
};

// Handles framebuffer resize debouncing to avoid recreating GPU objects
// every frame during panel resizing
class FramebufferResizeDebouncer {
public:
    static constexpr float DEFAULT_DEBOUNCE_SEC = 0.15f;

    FramebufferResizeDebouncer(float debounce_sec = DEFAULT_DEBOUNCE_SEC);

    bool should_resize(int current_w, int current_h, int desired_w, int desired_h, float dt);

    int target_width() const { return m_pending_width; }
    int target_height() const { return m_pending_height; }

    void set_failed() { m_failed = true; }

    void reset_failure() { m_failed = false; }

    bool has_failed() const { return m_failed; }

private:
    int m_pending_width = 0;
    int m_pending_height = 0;
    float m_timer = 0.0f;
    float m_debounce_sec;
    bool m_failed = false;
};

}

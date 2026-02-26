#pragma once

#include "Panel.h"
#include "editor/render/EditorTextureCache.h"
#include "editor/render/EditorTextRenderer.h"
#include "editor/render/PanelSceneRenderer.h"
#include "engine/rhi/RHIFramebuffer.h"
#include <entt/entt.hpp>
#include <imgui.h>
#include <string>
#include <memory>

namespace editor {

class EditorContext;

struct RefResolution {
    const char* name;
    float width;
    float height;
};

// Screen panel for viewing and manipulating screen-space entities
// Similar to Viewport but for UI entities using ScreenRect instead of Transform
class ScreenPanel : public Panel {
public:
    explicit ScreenPanel(EditorContext& context);
    ~ScreenPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    void reset_camera() { fit_to_canvas(); }
    void fit_to_canvas();

private:
    void create_framebuffer(int width, int height);
    void destroy_framebuffer();
    void render_scene();
    void render_entities(ImDrawList* draw_list, ImVec2 canvas_pos, ImVec2 canvas_size);
    void render_gizmos(ImDrawList* draw_list, ImVec2 canvas_pos, ImVec2 canvas_size);
    void render_toolbar();
    void handle_input(ImVec2 canvas_pos, ImVec2 canvas_size);

    ImVec2 screen_to_canvas(float sx, float sy, ImVec2 canvas_pos, ImVec2 canvas_size) const;
    void canvas_to_screen(ImVec2 canvas_pos_local, ImVec2 canvas_size, float& out_sx, float& out_sy) const;

    EditorContext& m_context;

    std::unique_ptr<engine::rhi::RHIFramebuffer> m_framebuffer;

    int m_canvas_width = 0;
    int m_canvas_height = 0;

    FramebufferResizeDebouncer m_resize_debouncer;

    float m_ref_width = 1920.0f;
    float m_ref_height = 1080.0f;
    int m_resolution_index = 0;

    static constexpr RefResolution RESOLUTIONS[] = {
        {"1920x1080 (Full HD)", 1920.0f, 1080.0f},
        {"1280x720 (HD)", 1280.0f, 720.0f},
        {"2560x1440 (QHD)", 2560.0f, 1440.0f},
        {"3840x2160 (4K)", 3840.0f, 2160.0f},
        {"1366x768", 1366.0f, 768.0f},
        {"1600x900", 1600.0f, 900.0f},
        {"800x600", 800.0f, 600.0f},
        {"1024x768", 1024.0f, 768.0f},
    };
    static constexpr int RESOLUTION_COUNT = sizeof(RESOLUTIONS) / sizeof(RESOLUTIONS[0]);

    float m_zoom = 1.0f;
    float m_pan_x = 0.0f;
    float m_pan_y = 0.0f;

    bool m_is_panning = false;
    float m_pan_start_mouse_x = 0.0f;
    float m_pan_start_mouse_y = 0.0f;
    float m_pan_start_offset_x = 0.0f;
    float m_pan_start_offset_y = 0.0f;

    bool m_is_dragging = false;
    entt::entity m_drag_entity = entt::null;
    float m_drag_start_x = 0.0f;
    float m_drag_start_y = 0.0f;
    float m_entity_start_offset_x = 0.0f;
    float m_entity_start_offset_y = 0.0f;

    EditorTextureCache m_image_textures;

    void render_image_entity(ImDrawList* draw_list, entt::entity entity, ImVec2 canvas_pos, ImVec2 canvas_size);
    void render_text_entity(ImDrawList* draw_list, entt::entity entity, ImVec2 canvas_pos, ImVec2 canvas_size);

    std::unique_ptr<EditorTextRenderer> m_text_renderer;
    void ensure_text_renderer();
};

}
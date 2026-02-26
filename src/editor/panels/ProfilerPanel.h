#pragma once

#include "Panel.h"
#include "engine/profiler/ProfilerTypes.h"
#include <imgui.h>
#include <vector>
#include <string>

namespace engine {
class Engine;
namespace profiler {
class GPUProfiler;
}
}

namespace editor {

class EditorContext;

// Profiler panel for capturing and displaying performance metrics
// Features flame chart, frame time graph, pan/zoom, and capture workflow
class ProfilerPanel : public Panel {
public:
    ProfilerPanel(engine::Engine& engine, EditorContext& context);
    ~ProfilerPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    void set_gpu_profiler(engine::profiler::GPUProfiler* profiler);

private:
    void render_toolbar();
    void render_overview_tab();
    void render_cpu_tab();
    void render_gpu_tab();
    void render_settings_popup();

    void render_flame_chart(const std::vector<engine::profiler::FrameData>& frames, bool is_gpu);
    void render_flame_chart_frame(const engine::profiler::FrameData& frame,
                                   float x_offset, float frame_width,
                                   float y_start, float row_height, bool is_gpu,
                                   const ImVec2& mouse_pos, ImVec2 region_min, ImVec2 region_max);

    void render_frame_time_graph(const std::vector<engine::profiler::FrameData>& frames);

    void handle_pan_zoom(const ImVec2& region_min, const ImVec2& region_size);
    void reset_view();

    ImU32 get_scope_color(const std::string& name, uint32_t depth) const;
    void export_capture();
    void import_capture();

    engine::Engine& m_engine;
    EditorContext& m_context;
    engine::profiler::GPUProfiler* m_gpu_profiler = nullptr;

    double m_view_start_frame = 0.0;
    double m_view_frame_count = 100.0;
    int m_selected_frame = -1;

    bool m_is_dragging = false;
    float m_drag_start_x = 0.0f;
    double m_drag_start_view = 0.0;

    std::vector<engine::profiler::ProfileNode> m_pending_gpu_nodes;
    double m_pending_gpu_time_ms = 0.0;

    float m_target_fps = 60.0f;
    bool m_show_gpu_in_flame = true;
    float m_flame_chart_height = 150.0f;
    float m_frame_graph_height = 80.0f;

    int m_selected_tab = 0;

    struct HoveredScope {
        bool valid = false;
        std::string name;
        double duration_ms = 0.0;
        double start_time_ms = 0.0;
        uint32_t depth = 0;
        int frame_index = -1;
    };
    HoveredScope m_hovered_scope;
};

}

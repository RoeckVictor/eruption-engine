#include "ProfilerPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "engine/core/Engine.h"
#include "engine/profiler/Profiler.h"
#include "engine/profiler/GPUProfiler.h"
#include "engine/platform/PlatformUtils.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace editor {

// Color palette for flame chart (hashed by scope name)
static const ImU32 s_flame_colors[] = {
    IM_COL32(66, 133, 244, 255),   // Blue
    IM_COL32(219, 68, 55, 255),    // Red
    IM_COL32(244, 180, 0, 255),    // Yellow
    IM_COL32(15, 157, 88, 255),    // Green
    IM_COL32(171, 71, 188, 255),   // Purple
    IM_COL32(255, 112, 67, 255),   // Orange
    IM_COL32(0, 172, 193, 255),    // Cyan
    IM_COL32(124, 179, 66, 255),   // Lime
    IM_COL32(103, 58, 183, 255),   // Deep Purple
    IM_COL32(233, 30, 99, 255),    // Pink
};
static const int s_flame_color_count = sizeof(s_flame_colors) / sizeof(s_flame_colors[0]);

ProfilerPanel::ProfilerPanel(engine::Engine& engine, EditorContext& context)
    : Panel("Profiler")
    , m_engine(engine)
    , m_context(context)
{
    // Set up play state callback for auto capture
    auto& profiler = engine::profiler::Profiler::instance();
    profiler.set_play_state_callback([this]() {
        return m_context.is_playing();
    });
}

ProfilerPanel::~ProfilerPanel() {
    // Clear callback
    engine::profiler::Profiler::instance().set_play_state_callback(nullptr);
}

void ProfilerPanel::on_open() {
    reset_view();
}

void ProfilerPanel::on_close() {
}

void ProfilerPanel::set_gpu_profiler(engine::profiler::GPUProfiler* profiler) {
    m_gpu_profiler = profiler;
}

void ProfilerPanel::on_gui() {
    auto& profiler = engine::profiler::Profiler::instance();

    // Collect GPU results if available
    if (m_gpu_profiler && m_gpu_profiler->is_supported()) {
        m_gpu_profiler->collect_results(m_pending_gpu_nodes, m_pending_gpu_time_ms);
    }

    // Copy GPU data to current frame if capturing
    if (profiler.is_capturing() && !m_pending_gpu_nodes.empty()) {
        // GPU nodes are added to the snapshot frames by the profiler
        // We just cache them here for live display
    }

    render_toolbar();
    ImGui::Separator();

    // Tab bar
    if (ImGui::BeginTabBar("ProfilerTabs")) {
        if (ImGui::BeginTabItem("Overview")) {
            m_selected_tab = 0;
            render_overview_tab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CPU")) {
            m_selected_tab = 1;
            render_cpu_tab();
            ImGui::EndTabItem();
        }
        if (m_gpu_profiler && m_gpu_profiler->is_supported()) {
            if (ImGui::BeginTabItem("GPU")) {
                m_selected_tab = 2;
                render_gpu_tab();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void ProfilerPanel::render_toolbar() {
    auto& profiler = engine::profiler::Profiler::instance();
    auto state = profiler.capture_state();
    const auto& snapshot = profiler.snapshot();

    // Capture state indicator
    if (state == engine::profiler::CaptureState::Recording) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::Text(ICON_FA_CIRCLE " Recording");
        ImGui::PopStyleColor();
    } else if (state == engine::profiler::CaptureState::Stopped && !snapshot.empty()) {
        ImGui::Text(ICON_FA_DATABASE " %zu frames captured (%.1f s)",
                    snapshot.frame_count(),
                    snapshot.total_duration_ms / 1000.0);
    } else {
        ImGui::TextDisabled("No capture");
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Record/Stop button
    if (state == engine::profiler::CaptureState::Recording) {
        if (ImGui::Button(ICON_FA_STOP " Stop")) {
            profiler.stop_capture();
        }
    } else {
        bool can_record = m_context.is_playing();
        if (!can_record) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(ICON_FA_CIRCLE " Record")) {
            profiler.start_capture();
        }
        if (!can_record) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Enter play mode to start recording");
            }
        }
    }

    ImGui::SameLine();

    // Clear button
    bool has_data = !snapshot.empty();
    if (!has_data) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_TRASH " Clear")) {
        profiler.clear_capture();
        reset_view();
    }
    if (!has_data) ImGui::EndDisabled();

    ImGui::SameLine();

    // Export button
    if (!has_data) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_UPLOAD " Export")) {
        export_capture();
    }
    if (!has_data) ImGui::EndDisabled();

    ImGui::SameLine();

    // Import button
    if (ImGui::Button(ICON_FA_DOWNLOAD " Import")) {
        import_capture();
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Stats
    if (!snapshot.empty()) {
        double avg_ms = profiler.average_frame_time_ms();
        double fps = profiler.fps();
        ImGui::Text("Avg: %.2f ms (%.1f FPS)", avg_ms, fps);
    } else {
        // Show live stats
        const auto& frame = profiler.current_frame();
        ImGui::Text("Live: %.2f ms", frame.total_frame_time_ms);
    }

    // Settings button (right-aligned)
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    if (ImGui::Button(ICON_FA_GEAR " Settings")) {
        ImGui::OpenPopup("ProfilerSettings");
    }

    render_settings_popup();
}

void ProfilerPanel::render_settings_popup() {
    auto& profiler = engine::profiler::Profiler::instance();
    auto& config = profiler.config();

    if (ImGui::BeginPopup("ProfilerSettings")) {
        ImGui::Text("Capture Settings");
        ImGui::Separator();

        ImGui::Checkbox("Auto-start on play", &config.auto_start_on_play);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically start recording when entering play mode");
        }

        ImGui::Checkbox("Auto-stop on play end", &config.auto_stop_on_play_end);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically stop recording when exiting play mode");
        }

        ImGui::Checkbox("Capture GPU timings", &config.capture_gpu);

        int max_frames = static_cast<int>(config.max_frames);
        if (ImGui::InputInt("Max frames", &max_frames, 100, 1000)) {
            config.max_frames = static_cast<size_t>(std::max(100, max_frames));
        }

        ImGui::Separator();
        ImGui::Text("Display Settings");
        ImGui::Separator();

        ImGui::SliderFloat("Target FPS", &m_target_fps, 30.0f, 144.0f);
        ImGui::SliderFloat("Flame chart height", &m_flame_chart_height, 80.0f, 300.0f);
        ImGui::SliderFloat("Frame graph height", &m_frame_graph_height, 40.0f, 150.0f);
        ImGui::Checkbox("Show GPU in flame chart", &m_show_gpu_in_flame);

        ImGui::EndPopup();
    }
}

void ProfilerPanel::render_overview_tab() {
    auto& profiler = engine::profiler::Profiler::instance();
    const auto& snapshot = profiler.snapshot();

    if (snapshot.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "No captured data. Enter play mode and click Record, or enable auto-start.");
        return;
    }

    // Flame chart
    ImGui::Text("Flame Chart");
    render_flame_chart(snapshot.frames, false);

    ImGui::Spacing();

    // Frame time graph
    ImGui::Text("Frame Time");
    render_frame_time_graph(snapshot.frames);

    // Frame info
    if (m_selected_frame >= 0 && m_selected_frame < static_cast<int>(snapshot.frames.size())) {
        ImGui::Separator();
        const auto& frame = snapshot.frames[m_selected_frame];
        ImGui::Text("Selected Frame %d: %.3f ms (CPU: %.3f ms, GPU: %.3f ms)",
                    m_selected_frame, frame.total_frame_time_ms,
                    frame.cpu_time_ms, frame.gpu_time_ms);
    }
}

void ProfilerPanel::render_flame_chart(const std::vector<engine::profiler::FrameData>& frames, bool is_gpu) {
    if (frames.empty()) return;

    ImVec2 region_min = ImGui::GetCursorScreenPos();
    ImVec2 region_size = ImVec2(ImGui::GetContentRegionAvail().x, m_flame_chart_height);
    ImVec2 region_max = ImVec2(region_min.x + region_size.x, region_min.y + region_size.y);

    // Reserve space
    ImGui::InvisibleButton("##FlameChartRegion", region_size);
    bool hovered = ImGui::IsItemHovered();

    // Handle pan/zoom
    handle_pan_zoom(region_min, region_size);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(region_min, region_max, IM_COL32(30, 30, 30, 255));

    // Calculate visible frame range
    int start_frame = static_cast<int>(m_view_start_frame);
    int end_frame = static_cast<int>(m_view_start_frame + m_view_frame_count) + 1;
    start_frame = std::max(0, start_frame);
    end_frame = std::min(static_cast<int>(frames.size()), end_frame);

    if (start_frame >= end_frame) return;

    float frame_width = region_size.x / static_cast<float>(m_view_frame_count);
    float row_height = 16.0f;

    // Reset hovered scope before drawing
    m_hovered_scope.valid = false;

    // Get mouse position for hover detection
    ImVec2 mouse = ImGui::GetMousePos();

    // Draw frames and detect hover
    for (int i = start_frame; i < end_frame; ++i) {
        float x_offset = region_min.x + (i - static_cast<float>(m_view_start_frame)) * frame_width;
        render_flame_chart_frame(frames[i], x_offset, frame_width, region_min.y, row_height, is_gpu,
                                  mouse, region_min, region_max);
        if (m_hovered_scope.valid && m_hovered_scope.frame_index < 0) {
            m_hovered_scope.frame_index = i;
        }
    }

    // Draw frame separators
    for (int i = start_frame; i <= end_frame; ++i) {
        float x = region_min.x + (i - static_cast<float>(m_view_start_frame)) * frame_width;
        if (x >= region_min.x && x <= region_min.x + region_size.x) {
            draw_list->AddLine(
                ImVec2(x, region_min.y),
                ImVec2(x, region_min.y + region_size.y),
                IM_COL32(60, 60, 60, 255), 1.0f);
        }
    }

    // Selected frame highlight
    if (m_selected_frame >= start_frame && m_selected_frame < end_frame) {
        float x = region_min.x + (m_selected_frame - static_cast<float>(m_view_start_frame)) * frame_width;
        draw_list->AddRectFilled(
            ImVec2(x, region_min.y),
            ImVec2(x + frame_width, region_min.y + region_size.y),
            IM_COL32(255, 255, 255, 30));
    }

    // Handle click to select frame
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyCtrl) {
        float rel_x = mouse.x - region_min.x;
        int clicked_frame = static_cast<int>(m_view_start_frame + rel_x / frame_width);
        if (clicked_frame >= 0 && clicked_frame < static_cast<int>(frames.size())) {
            m_selected_frame = clicked_frame;
        }
    }

    // Tooltip - show scope info if hovering a scope, otherwise show frame info
    if (hovered) {
        if (m_hovered_scope.valid) {
            // Show detailed scope tooltip
            ImGui::BeginTooltip();
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "%s", m_hovered_scope.name.c_str());
            ImGui::Separator();
            ImGui::Text("Duration: %.3f ms", m_hovered_scope.duration_ms);
            ImGui::Text("Start:    %.3f ms", m_hovered_scope.start_time_ms);
            ImGui::Text("Depth:    %u", m_hovered_scope.depth);
            ImGui::Text("Frame:    %d", m_hovered_scope.frame_index);
            ImGui::EndTooltip();
        } else {
            // Show basic frame tooltip
            float rel_x = mouse.x - region_min.x;
            int hover_frame = static_cast<int>(m_view_start_frame + rel_x / frame_width);
            if (hover_frame >= 0 && hover_frame < static_cast<int>(frames.size())) {
                const auto& frame = frames[hover_frame];
                ImGui::BeginTooltip();
                ImGui::Text("Frame %d", hover_frame);
                ImGui::Text("Total: %.3f ms", frame.total_frame_time_ms);
                ImGui::Text("CPU: %.3f ms", frame.cpu_time_ms);
                if (frame.gpu_time_ms > 0) {
                    ImGui::Text("GPU: %.3f ms", frame.gpu_time_ms);
                }
                ImGui::EndTooltip();
            }
        }
    }

    // Border
    draw_list->AddRect(region_min, region_max, IM_COL32(80, 80, 80, 255));
}

void ProfilerPanel::render_flame_chart_frame(const engine::profiler::FrameData& frame,
                                              float x_offset, float frame_width,
                                              float y_start, float row_height, bool is_gpu,
                                              const ImVec2& mouse_pos, ImVec2 region_min, ImVec2 region_max) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const auto& nodes = is_gpu ? frame.gpu_nodes : frame.cpu_nodes;

    if (nodes.empty()) return;

    // Find max depth
    uint32_t max_depth = 0;
    for (const auto& node : nodes) {
        max_depth = std::max(max_depth, node.depth);
    }

    // Frame time for scaling
    double frame_time = frame.total_frame_time_ms;
    if (frame_time <= 0.0) frame_time = 1.0;

    // Draw each node
    for (const auto& node : nodes) {
        float x = x_offset + static_cast<float>(node.start_time_ms / frame_time) * frame_width;
        float w = static_cast<float>(node.duration_ms / frame_time) * frame_width;
        float y = y_start + node.depth * row_height;

        // Skip if too small to see
        if (w < 1.0f) continue;

        // Calculate rect bounds
        ImVec2 rect_min(x + 0.5f, y + 0.5f);
        ImVec2 rect_max(x + w - 0.5f, y + row_height - 0.5f);

        // Check if mouse is hovering this node (only if within visible region)
        bool is_hovered = !m_hovered_scope.valid &&
                          mouse_pos.x >= rect_min.x && mouse_pos.x <= rect_max.x &&
                          mouse_pos.y >= rect_min.y && mouse_pos.y <= rect_max.y &&
                          mouse_pos.x >= region_min.x && mouse_pos.x <= region_max.x &&
                          mouse_pos.y >= region_min.y && mouse_pos.y <= region_max.y;

        if (is_hovered) {
            m_hovered_scope.valid = true;
            m_hovered_scope.name = node.name;
            m_hovered_scope.duration_ms = node.duration_ms;
            m_hovered_scope.start_time_ms = node.start_time_ms;
            m_hovered_scope.depth = node.depth;
            m_hovered_scope.frame_index = -1;  // Set by caller
        }

        ImU32 color = get_scope_color(node.name, node.depth);

        // Brighten if hovered
        if (is_hovered) {
            int r = (color >> 0) & 0xFF;
            int g = (color >> 8) & 0xFF;
            int b = (color >> 16) & 0xFF;
            r = std::min(255, r + 40);
            g = std::min(255, g + 40);
            b = std::min(255, b + 40);
            color = IM_COL32(r, g, b, 255);
        }

        // Draw bar
        draw_list->AddRectFilled(rect_min, rect_max, color);

        // Draw highlight border if hovered
        if (is_hovered) {
            draw_list->AddRect(rect_min, rect_max, IM_COL32(255, 255, 255, 200), 0.0f, 0, 2.0f);
        }

        // Draw label if bar is wide enough
        if (w > 30.0f) {
            const char* label = node.name.c_str();
            ImVec2 text_size = ImGui::CalcTextSize(label);
            if (text_size.x < w - 4.0f) {
                draw_list->AddText(
                    ImVec2(x + 2.0f, y + 1.0f),
                    IM_COL32(255, 255, 255, 255),
                    label);
            }
        }
    }
}

void ProfilerPanel::render_frame_time_graph(const std::vector<engine::profiler::FrameData>& frames) {
    if (frames.empty()) return;

    ImVec2 region_min = ImGui::GetCursorScreenPos();
    ImVec2 region_size = ImVec2(ImGui::GetContentRegionAvail().x, m_frame_graph_height);

    // Reserve space
    ImGui::InvisibleButton("##FrameGraphRegion", region_size);
    bool hovered = ImGui::IsItemHovered();

    // Handle pan/zoom (shared with flame chart)
    handle_pan_zoom(region_min, region_size);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(region_min,
        ImVec2(region_min.x + region_size.x, region_min.y + region_size.y),
        IM_COL32(30, 30, 30, 255));

    // Calculate visible frame range
    int start_frame = static_cast<int>(m_view_start_frame);
    int end_frame = static_cast<int>(m_view_start_frame + m_view_frame_count) + 1;
    start_frame = std::max(0, start_frame);
    end_frame = std::min(static_cast<int>(frames.size()), end_frame);

    if (start_frame >= end_frame) return;

    // Find max frame time for scaling
    float target_ms = 1000.0f / m_target_fps;
    float max_ms = target_ms * 2.0f;
    for (int i = start_frame; i < end_frame; ++i) {
        max_ms = std::max(max_ms, static_cast<float>(frames[i].total_frame_time_ms));
    }
    max_ms *= 1.1f;  // Add some headroom

    float frame_width = region_size.x / static_cast<float>(m_view_frame_count);

    // Draw bars
    for (int i = start_frame; i < end_frame; ++i) {
        float x = region_min.x + (i - static_cast<float>(m_view_start_frame)) * frame_width;
        float h = static_cast<float>(frames[i].total_frame_time_ms / max_ms) * region_size.y;
        float y = region_min.y + region_size.y - h;

        // Color based on frame time
        ImU32 color;
        float frame_ms = static_cast<float>(frames[i].total_frame_time_ms);
        if (frame_ms > target_ms * 1.5f) {
            color = IM_COL32(255, 80, 80, 255);  // Red
        } else if (frame_ms > target_ms) {
            color = IM_COL32(255, 200, 80, 255); // Orange
        } else {
            color = IM_COL32(80, 200, 80, 255);  // Green
        }

        draw_list->AddRectFilled(
            ImVec2(x + 1.0f, y),
            ImVec2(x + frame_width - 1.0f, region_min.y + region_size.y),
            color);
    }

    // Target line
    float target_y = region_min.y + region_size.y * (1.0f - target_ms / max_ms);
    draw_list->AddLine(
        ImVec2(region_min.x, target_y),
        ImVec2(region_min.x + region_size.x, target_y),
        IM_COL32(0, 255, 0, 128), 1.0f);

    // Selected frame marker
    if (m_selected_frame >= start_frame && m_selected_frame < end_frame) {
        float x = region_min.x + (m_selected_frame - static_cast<float>(m_view_start_frame)) * frame_width;
        draw_list->AddRectFilled(
            ImVec2(x, region_min.y),
            ImVec2(x + frame_width, region_min.y + region_size.y),
            IM_COL32(255, 255, 255, 30));
    }

    // Handle click to select frame
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyCtrl) {
        ImVec2 mouse = ImGui::GetMousePos();
        float rel_x = mouse.x - region_min.x;
        int clicked_frame = static_cast<int>(m_view_start_frame + rel_x / frame_width);
        if (clicked_frame >= 0 && clicked_frame < static_cast<int>(frames.size())) {
            m_selected_frame = clicked_frame;
        }
    }

    // Border
    draw_list->AddRect(region_min,
        ImVec2(region_min.x + region_size.x, region_min.y + region_size.y),
        IM_COL32(80, 80, 80, 255));
}

void ProfilerPanel::handle_pan_zoom(const ImVec2& region_min, const ImVec2& region_size) {
    auto& profiler = engine::profiler::Profiler::instance();
    const auto& snapshot = profiler.snapshot();
    if (snapshot.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    bool hovered = ImGui::IsItemHovered();

    // Zoom with scroll wheel
    if (hovered && std::abs(io.MouseWheel) > 0.0f) {
        float zoom_factor = 1.0f - io.MouseWheel * 0.1f;
        zoom_factor = std::clamp(zoom_factor, 0.5f, 2.0f);

        // Zoom towards mouse position
        ImVec2 mouse = ImGui::GetMousePos();
        float rel_x = (mouse.x - region_min.x) / region_size.x;
        double frame_at_mouse = m_view_start_frame + m_view_frame_count * rel_x;

        m_view_frame_count *= zoom_factor;
        m_view_frame_count = std::clamp(m_view_frame_count, 10.0, static_cast<double>(snapshot.frame_count()));

        // Adjust start to keep mouse position stable
        m_view_start_frame = frame_at_mouse - m_view_frame_count * rel_x;
    }

    // Pan with Ctrl+drag or middle mouse
    bool pan_active = (io.KeyCtrl && ImGui::IsMouseDown(ImGuiMouseButton_Left)) ||
                      ImGui::IsMouseDown(ImGuiMouseButton_Middle);

    if (hovered && pan_active && !m_is_dragging) {
        m_is_dragging = true;
        m_drag_start_x = io.MousePos.x;
        m_drag_start_view = m_view_start_frame;
    }

    if (m_is_dragging) {
        if (pan_active) {
            float dx = io.MousePos.x - m_drag_start_x;
            float frames_per_pixel = static_cast<float>(m_view_frame_count) / region_size.x;
            m_view_start_frame = m_drag_start_view - dx * frames_per_pixel;
        } else {
            m_is_dragging = false;
        }
    }

    // Clamp view
    m_view_start_frame = std::clamp(m_view_start_frame, 0.0,
        std::max(0.0, static_cast<double>(snapshot.frame_count()) - m_view_frame_count));
}

void ProfilerPanel::reset_view() {
    auto& profiler = engine::profiler::Profiler::instance();
    const auto& snapshot = profiler.snapshot();

    m_view_start_frame = 0.0;
    m_view_frame_count = snapshot.empty() ? 100.0 : static_cast<double>(snapshot.frame_count());
    m_view_frame_count = std::min(m_view_frame_count, 500.0);  // Max initial view
    m_selected_frame = -1;
}

ImU32 ProfilerPanel::get_scope_color(const std::string& name, uint32_t depth) const {
    // Hash the name to get consistent colors
    std::hash<std::string> hasher;
    size_t hash = hasher(name);
    int index = static_cast<int>(hash % s_flame_color_count);

    // Darken based on depth
    ImU32 base = s_flame_colors[index];
    int r = (base >> 0) & 0xFF;
    int g = (base >> 8) & 0xFF;
    int b = (base >> 16) & 0xFF;

    float darken = 1.0f - depth * 0.1f;
    darken = std::max(0.5f, darken);

    r = static_cast<int>(r * darken);
    g = static_cast<int>(g * darken);
    b = static_cast<int>(b * darken);

    return IM_COL32(r, g, b, 255);
}

void ProfilerPanel::export_capture() {
    auto& profiler = engine::profiler::Profiler::instance();

    std::vector<engine::platform::FileFilter> filters = {
        {"JSON Files", "*.json"}
    };

    std::string path = engine::platform::save_file_dialog(
        "Export Profiler Capture",
        filters,
        ".json");

    if (!path.empty()) {
        profiler.export_to_json(path.c_str());
    }
}

void ProfilerPanel::import_capture() {
    auto& profiler = engine::profiler::Profiler::instance();

    std::vector<engine::platform::FileFilter> filters = {
        {"JSON Files", "*.json"}
    };

    std::string path = engine::platform::open_file_dialog(
        "Import Profiler Capture",
        filters);

    if (!path.empty()) {
        if (profiler.import_from_json(path.c_str())) {
            reset_view();
        }
    }
}

void ProfilerPanel::render_cpu_tab() {
    auto& profiler = engine::profiler::Profiler::instance();
    const auto& snapshot = profiler.snapshot();

    if (snapshot.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No captured data.");
        return;
    }

    // Summary table
    auto summaries = profiler.get_summaries();

    if (ImGui::BeginTable("CPUSummary", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_Sortable |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
            ImVec2(0, 200)))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Avg (ms)");
        ImGui::TableSetupColumn("Min (ms)");
        ImGui::TableSetupColumn("Max (ms)");
        ImGui::TableSetupColumn("Calls");
        ImGui::TableHeadersRow();

        for (const auto& summary : summaries) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", summary.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", summary.avg_ms);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", summary.min_ms);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.3f", summary.max_ms);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u", summary.call_count);
        }

        ImGui::EndTable();
    }

    // Selected frame detail
    if (m_selected_frame >= 0 && m_selected_frame < static_cast<int>(snapshot.frames.size())) {
        ImGui::Separator();
        ImGui::Text("Frame %d Hierarchy", m_selected_frame);

        const auto& frame = snapshot.frames[m_selected_frame];
        if (!frame.cpu_nodes.empty()) {
            // Simple tree view of selected frame
            for (const auto& node : frame.cpu_nodes) {
                if (node.depth == 0) {
                    ImGui::BulletText("%s: %.3f ms", node.name.c_str(), node.duration_ms);
                }
            }
        }
    }
}

void ProfilerPanel::render_gpu_tab() {
    auto& profiler = engine::profiler::Profiler::instance();
    const auto& snapshot = profiler.snapshot();

    if (!m_gpu_profiler || !m_gpu_profiler->is_supported()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "GPU profiling not available");
        return;
    }

    if (snapshot.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No captured data.");
        return;
    }

    ImGui::Text("GPU data latency: %u frames", m_gpu_profiler->result_latency());

    // GPU flame chart
    render_flame_chart(snapshot.frames, true);
}

} // namespace editor

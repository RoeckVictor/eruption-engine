#pragma once

#include <imgui.h>

namespace editor {

class EditorContext;
class RuntimeContext;
class ScriptManager;
class PanelManager;

/// Renders the main editor toolbar (play/pause/stop, gizmo modes, grid, snap, etc.).
/// Extracted from EditorApplication to reduce its size and keep toolbar logic isolated.
class EditorToolbar {
public:
    EditorToolbar(EditorContext& context, RuntimeContext& runtime,
                  ScriptManager& scripts, PanelManager& panels);

    /// Render the toolbar. Sets the toolbar height on PanelManager.
    void render();

private:
    void render_play_controls();
    void render_gizmo_mode_buttons();
    void render_grid_snap_controls();
    void render_gizmo_visibility_popup();
    void render_script_status();

    EditorContext& m_context;
    RuntimeContext& m_runtime;
    ScriptManager& m_scripts;
    PanelManager& m_panels;
};

} // namespace editor

#pragma once

#include "Panel.h"
#include "engine/simulation/MaterialDefinition.h"
#include "engine/simulation/CategoryDefinition.h"
#include "engine/simulation/CategoryLibrary.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace engine::simulation {
class MaterialLibrary;
}

namespace editor {

class EditorContext;

/// Material editor panel for editing .material files visually.
///
/// Features:
/// - Material list with search/filter
/// - Property editor (physical, thermal, rendering)
/// - Interaction list with add/remove/edit
/// - Save/reload functionality
/// - Create new material
/// - Graph visualization of material transformation relationships
class MaterialEditorPanel : public Panel {
public:
    explicit MaterialEditorPanel(EditorContext& context);
    ~MaterialEditorPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    /// Set the material library to edit.
    void set_library(engine::simulation::MaterialLibrary* library);

    /// Set the category library to edit.
    void set_category_library(engine::simulation::CategoryLibrary* library);

    /// Reload materials from directory.
    void reload_materials();

    /// Reload categories from directory.
    void reload_categories();

    /// Open the new material dialog (can be called externally).
    void open_new_material_dialog();

    /// Open the new category dialog (can be called externally).
    void open_new_category_dialog();

    /// Select a material by its file path and switch to Editor tab.
    void select_material_by_path(const std::string& path);

    /// Select a category by its file path and switch to Categories tab.
    void select_category_by_path(const std::string& path);

private:
    int find_smallest_unused_id() const;
    bool is_id_taken(int id) const;
    void render_toolbar();
    void render_material_list();
    void render_material_properties();
    void render_physical_section(bool readonly);
    void render_thermal_section(bool readonly);
    void render_rendering_section(bool readonly);
    void render_flags_section(bool readonly);
    void render_interactions_section(bool readonly);
    void render_interaction_editor(engine::simulation::Interaction& interaction, int index, bool readonly);
    void render_effect_editor(engine::simulation::InteractionEffect& effect, int index, bool readonly);
    void render_conditions_editor(engine::simulation::InteractionConditions& conditions, bool readonly);

    void save_current_material();
    void create_new_material();
    void delete_current_material();
    void delete_material(uint8_t material_id);

    // Graph visualization
    void render_graph_view();
    void build_graph();
    void layout_graph();
    void draw_material_node(const engine::simulation::MaterialDefinition& mat, ImVec2 pos, bool selected);
    void draw_interaction_edge(ImVec2 from, ImVec2 to, uint8_t from_id, uint8_t to_id,
                               engine::simulation::InteractionType type,
                               int parallel_index, int parallel_count, bool highlighted = false);

    // Categories tab
    void render_categories_tab();
    void render_category_list();
    void render_category_properties();
    void render_category_preview(const engine::simulation::CategoryDefinition& cat);
    void render_movement_rules_editor(engine::simulation::CategoryDefinition& cat, bool readonly);
    void render_dissipation_editor(engine::simulation::CategoryDefinition& cat, bool readonly);
    void draw_direction_arrow(ImDrawList* dl, ImVec2 center, engine::simulation::Direction dir,
                               uint8_t priority, bool density_check, float scale);
    void save_current_category();
    void create_new_category();
    void delete_category(uint8_t category_id);

    // Helper to convert ImGui color to uint32_t
    static uint32_t color_to_uint32(const float color[4]);
    // Helper to convert uint32_t to ImGui color
    static void uint32_to_color(uint32_t color, float out[4]);

    EditorContext& m_context;
    engine::simulation::MaterialLibrary* m_library = nullptr;
    engine::simulation::CategoryLibrary m_owned_category_library;  // Owned by the panel
    engine::simulation::CategoryLibrary* m_category_library = nullptr;  // Points to m_owned or external

    // Currently selected material index
    int m_selected_material = -1;

    // Working copy of the selected material (for editing)
    engine::simulation::MaterialDefinition m_editing_material;
    bool m_has_unsaved_changes = false;

    // Material directory path
    std::string m_materials_path;

    // Filter string for material list
    char m_filter_text[256] = "";

    // Expanded states for collapsing headers
    bool m_physical_expanded = true;
    bool m_thermal_expanded = true;
    bool m_rendering_expanded = true;
    bool m_flags_expanded = false;
    bool m_interactions_expanded = true;

    // Interaction editor state
    int m_selected_interaction = -1;
    int m_selected_effect = -1;

    // New material dialog state
    bool m_show_new_material_dialog = false;
    char m_new_material_name[64] = "";
    char m_new_material_internal_name[64] = "";
    int m_new_material_id = 10;

    // Delete confirmation state
    int m_pending_delete_material_id = -1;
    std::string m_pending_delete_material_name;

    // Tab state
    int m_current_tab = 0;  // 0 = Editor, 1 = Graph, 2 = Categories

    // Category editing state
    int m_selected_category = -1;
    engine::simulation::CategoryDefinition m_editing_category;
    bool m_has_unsaved_category_changes = false;
    std::string m_categories_path;
    char m_category_filter_text[256] = "";

    // New category dialog state
    bool m_show_new_category_dialog = false;
    char m_new_category_name[64] = "";
    char m_new_category_internal_name[64] = "";
    int m_new_category_id = 1;

    // Category delete confirmation
    int m_pending_delete_category_id = -1;
    std::string m_pending_delete_category_name;

    // Graph visualization state
    struct GraphNode {
        uint8_t material_id;
        ImVec2 position;
        ImVec2 velocity;
    };

    struct GraphEdge {
        uint8_t from_id;
        uint8_t to_id;
        engine::simulation::InteractionType type;
        // Store interaction details for tooltip
        std::string interaction_id;
        uint8_t priority;
        float probability;
        uint16_t sim_step_threshold;
        uint8_t temp_above;
        uint8_t temp_below;
        std::vector<std::string> contact_with;
        std::string effect_desc;
        // For separating parallel edges visually
        int parallel_index = 0;   // Which edge this is among parallel edges (0, 1, 2, ...)
        int parallel_count = 1;   // Total number of parallel edges between these two nodes
    };

    std::vector<GraphNode> m_graph_nodes;
    std::vector<GraphEdge> m_graph_edges;
    bool m_graph_needs_rebuild = true;
    bool m_graph_needs_layout = true;

    // Graph view state
    ImVec2 m_graph_offset = ImVec2(0, 0);
    float m_graph_zoom = 1.0f;
    int m_graph_hovered_node = -1;
    int m_graph_hovered_edge = -1;
    int m_graph_dragging_node = -1;
    ImVec2 m_graph_drag_offset;

    // Helper to check if selected material is read-only (engine material)
    bool is_selected_material_readonly() const;

    // Helper to check if selected category is read-only (engine category)
    bool is_selected_category_readonly() const;

    static constexpr float NODE_RADIUS = 30.0f;
};

}

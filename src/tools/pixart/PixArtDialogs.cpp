#include "PixArtApp.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <string>

#include "engine/platform/PlatformUtils.h"

namespace pixart {

static const std::vector<engine::platform::FileFilter> PXG_FILTERS = {
    {"Pixel Grid Files (*.pxg)", "*.pxg"},
    {"All Files (*.*)", "*.*"}
};

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

void PixArtApp::render_new_dialog() {
    if (!m_show_new_dialog) return;

    ImGui::OpenPopup("New Document");
    if (ImGui::BeginPopupModal("New Document", &m_show_new_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputInt("Width", &m_new_width);
        ImGui::InputInt("Height", &m_new_height);
        m_new_width = std::clamp(m_new_width, 1, 4096);
        m_new_height = std::clamp(m_new_height, 1, 4096);

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Delete old texture
            if (m_canvas_tex) {
                glDeleteTextures(1, &m_canvas_tex);
                m_canvas_tex = 0;
            }
            m_doc.create(m_new_width, m_new_height);
            m_canvas_dirty = true;
            m_active_layer = 0;
            m_current_path.clear();
            m_has_unsaved_changes = false;
            m_undo.clear();
            m_view.reset();
            m_show_new_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_new_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PixArtApp::render_resize_dialog() {
    if (!m_show_resize_dialog) return;

    ImGui::OpenPopup("Resize Grid");
    if (ImGui::BeginPopupModal("Resize Grid", &m_show_resize_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputInt("Width", &m_resize_width);
        ImGui::InputInt("Height", &m_resize_height);
        m_resize_width = std::clamp(m_resize_width, 1, 4096);
        m_resize_height = std::clamp(m_resize_height, 1, 4096);

        if (ImGui::Button("Resize", ImVec2(120, 0))) {
            if (m_canvas_tex) {
                glDeleteTextures(1, &m_canvas_tex);
                m_canvas_tex = 0;
            }
            // Capture all pixels before resize for undo
            m_undo.begin_operation(m_doc);
            for (int ly = 0; ly < m_doc.height(); ++ly) {
                for (int lx = 0; lx < m_doc.width(); ++lx) {
                    for (int li = 0; li < m_doc.layer_count(); ++li) {
                        m_undo.record_pixel(li, lx, ly);
                    }
                }
            }
            m_doc.resize(m_resize_width, m_resize_height);
            m_undo.end_operation();
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
            m_show_resize_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_resize_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PixArtApp::render_add_layer_dialog() {
    if (!m_show_add_layer_dialog) return;

    ImGui::OpenPopup("Add Layer");
    if (ImGui::BeginPopupModal("Add Layer", &m_show_add_layer_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", m_new_layer_name, sizeof(m_new_layer_name));

        const char* types[] = {"UInt8", "Enum"};
        ImGui::Combo("Type", &m_new_layer_type, types, 2);

        if (m_new_layer_type == 1) {
            ImGui::Text("Enum values (comma-separated):");
            ImGui::InputText("##enumvals", m_enum_values_buf, sizeof(m_enum_values_buf));
        }

        bool valid_name = (m_new_layer_name[0] != '\0');

        if (!valid_name) ImGui::BeginDisabled();
        if (ImGui::Button("Add", ImVec2(120, 0))) {
            LayerType type = (m_new_layer_type == 1) ? LayerType::Enum : LayerType::UInt8;
            std::vector<std::string> enum_names;

            if (type == LayerType::Enum) {
                // Parse comma-separated enum values
                std::string buf(m_enum_values_buf);
                size_t pos = 0;
                while (pos < buf.size()) {
                    size_t comma = buf.find(',', pos);
                    if (comma == std::string::npos) comma = buf.size();
                    std::string val = buf.substr(pos, comma - pos);
                    // Trim whitespace
                    size_t start = val.find_first_not_of(" \t");
                    size_t end = val.find_last_not_of(" \t");
                    if (start != std::string::npos && end != std::string::npos) {
                        enum_names.push_back(val.substr(start, end - start + 1));
                    }
                    pos = comma + 1;
                }
            }

            m_doc.add_layer(m_new_layer_name, type, enum_names);
            m_canvas_dirty = true;
            m_show_add_layer_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        if (!valid_name) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_add_layer_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// File save helpers
// ---------------------------------------------------------------------------

bool PixArtApp::try_save_document() {
    if (m_current_path.empty()) {
        return try_save_document_as();
    }
    if (m_doc.save(m_current_path)) {
        m_has_unsaved_changes = false;
        return true;
    }
    return false;
}

bool PixArtApp::try_save_document_as() {
    std::string path = engine::platform::save_file_dialog("Save Pixel Grid", PXG_FILTERS, "pxg");
    if (!path.empty() && m_doc.save(path)) {
        m_current_path = path;
        m_has_unsaved_changes = false;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Unsaved changes handling
// ---------------------------------------------------------------------------

void PixArtApp::try_new_document() {
    if (m_has_unsaved_changes) {
        m_pending_action = PendingAction::New;
        m_show_unsaved_dialog = true;
    } else {
        do_new_document();
    }
}

void PixArtApp::try_open_file() {
    if (m_has_unsaved_changes) {
        m_pending_action = PendingAction::Open;
        m_show_unsaved_dialog = true;
    } else {
        do_open_file();
    }
}

void PixArtApp::try_exit() {
    if (m_has_unsaved_changes) {
        m_pending_action = PendingAction::Exit;
        m_show_unsaved_dialog = true;
    } else {
        m_should_close = true;
    }
}

void PixArtApp::do_new_document() {
    m_show_new_dialog = true;
    m_new_width = 16;
    m_new_height = 16;
}

void PixArtApp::do_open_file() {
    std::string path = engine::platform::open_file_dialog("Open Pixel Grid", PXG_FILTERS);
    if (!path.empty() && m_doc.load(path)) {
        if (m_canvas_tex) {
            glDeleteTextures(1, &m_canvas_tex);
            m_canvas_tex = 0;
        }
        m_current_path = path;
        m_canvas_dirty = true;
        m_active_layer = 0;
        m_has_unsaved_changes = false;
        m_undo.clear();
    }
}

void PixArtApp::render_unsaved_changes_dialog() {
    if (!m_show_unsaved_dialog) return;

    ImGui::OpenPopup("Unsaved Changes");
    if (ImGui::BeginPopupModal("Unsaved Changes", &m_show_unsaved_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Do you want to save before continuing?");
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            if (try_save_document()) {
                // Proceed with pending action
                PendingAction action = m_pending_action;
                m_pending_action = PendingAction::None;
                m_show_unsaved_dialog = false;
                ImGui::CloseCurrentPopup();

                if (action == PendingAction::New) do_new_document();
                else if (action == PendingAction::Open) do_open_file();
                else if (action == PendingAction::Exit) m_should_close = true;
            }
            // If save failed, stay in dialog
        }

        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
            // Proceed without saving
            PendingAction action = m_pending_action;
            m_pending_action = PendingAction::None;
            m_has_unsaved_changes = false;  // Discard changes
            m_show_unsaved_dialog = false;
            ImGui::CloseCurrentPopup();

            if (action == PendingAction::New) do_new_document();
            else if (action == PendingAction::Open) do_open_file();
            else if (action == PendingAction::Exit) m_should_close = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_pending_action = PendingAction::None;
            m_show_unsaved_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace pixart

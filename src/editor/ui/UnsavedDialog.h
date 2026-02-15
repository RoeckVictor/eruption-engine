#pragma once

namespace editor::ui {

/// Result from the unsaved changes popup.
enum class UnsavedAction { None, Save, DontSave, Cancel };

/// Render a standard "Unsaved Changes" modal popup with Save / Don't Save / Cancel buttons.
/// Returns which button was clicked (None if popup not open or no button pressed).
/// Caller must call ImGui::OpenPopup(popup_id) to trigger the popup before calling this.
UnsavedAction render_unsaved_popup(const char* popup_id, const char* message);

} // namespace editor::ui

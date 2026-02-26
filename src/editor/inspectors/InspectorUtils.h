#pragma once

#include <imgui.h>
#include <string>

namespace editor {

/// Callback structure for ImGui string editing with dynamic resize.
struct InputTextCallback {
    static int resize(ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string* str = static_cast<std::string*>(data->UserData);
            str->resize(data->BufTextLen);
            data->Buf = str->data();
        }
        return 0;
    }
};

/// Draw a single-line text input that works directly with std::string.
/// Avoids static buffer issues by using ImGui's callback resize.
inline bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    // Reserve space if string is empty to ensure a valid buffer
    if (str->empty()) {
        str->reserve(64);
    }
    return ImGui::InputText(label, str->data(), str->capacity() + 1, flags,
                            InputTextCallback::resize, str);
}

/// Draw a multiline text input that works directly with std::string.
inline bool InputTextMultiline(const char* label, std::string* str,
                                const ImVec2& size = ImVec2(0, 0),
                                ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    if (str->empty()) {
        str->reserve(256);
    }
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags,
                                      InputTextCallback::resize, str);
}

/// Draw a spacing + separator + spacing pattern commonly used between sections.
inline void SectionSeparator() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

/// Draw a labeled property with consistent layout (label on left, value at column 120).
/// Use with ImGui::PushID/PopID around the property.
inline void PropertyLabel(const char* label) {
    ImGui::Text("%s", label);
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
}

/// Helper to draw enabled checkbox - common pattern in component inspectors.
/// Returns true if enabled state changed.
inline bool EnabledCheckbox(bool* enabled) {
    return ImGui::Checkbox("Enabled", enabled);
}

/// Helper to draw render layer input - common pattern in renderable components.
/// Returns true if layer changed.
inline bool RenderLayerInput(int* layer) {
    return ImGui::DragInt("Render Layer", layer);
}

/// Helper to draw color editor from separate r,g,b,a floats.
/// Returns true if any color component changed.
inline bool ColorEdit4(const char* label, float* r, float* g, float* b, float* a) {
    float color[4] = { *r, *g, *b, *a };
    if (ImGui::ColorEdit4(label, color)) {
        *r = color[0];
        *g = color[1];
        *b = color[2];
        *a = color[3];
        return true;
    }
    return false;
}

} // namespace editor

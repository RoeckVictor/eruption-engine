#include "AutoInspector.h"
#include "InspectorUtils.h"
#include <imgui.h>
#include <string>

using namespace engine::reflection;

namespace editor {

// Static recording context
const RecordingContext* AutoInspector::s_recording_context = nullptr;

// Helper to check if a property is animated and push red background if so
static bool push_animated_background(const RecordingContext* ctx, const std::string& prop_name) {
    if (!ctx || !ctx->is_animated_callback) {
        return false;
    }

    std::string property_path = ctx->component_name + "." + prop_name;
    if (ctx->is_animated_callback(ctx->entity, property_path)) {
        // Push red-tinted background color for animated properties
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.7f, 0.25f, 0.25f, 1.0f));
        return true;
    }
    return false;
}

// Helper to pop the animated background if it was pushed
static void pop_animated_background(bool was_pushed) {
    if (was_pushed) {
        ImGui::PopStyleColor(3);
    }
}

// RAII guard for property drawing boilerplate (PushID, label, animated background)
struct PropertyGuard {
    bool animated = false;

    PropertyGuard(const PropertyInfo& prop, const RecordingContext* ctx) {
        ImGui::PushID(prop.name.c_str());
        PropertyLabel(prop.display_name.c_str());
        animated = push_animated_background(ctx, prop.name);
    }

    ~PropertyGuard() {
        pop_animated_background(animated);
        ImGui::PopID();
    }

    PropertyGuard(const PropertyGuard&) = delete;
    PropertyGuard& operator=(const PropertyGuard&) = delete;
};

void AutoInspector::set_recording_context(const RecordingContext* ctx) {
    s_recording_context = ctx;
}

bool AutoInspector::try_record(const std::string& prop_name,
                               const engine::animation::PropertyValue& value,
                               engine::animation::PropertyValueType type) {
    if (!s_recording_context || !s_recording_context->record_callback) {
        return false;
    }

    // Build property path: "Component.Property"
    std::string property_path = s_recording_context->component_name + "." + prop_name;

    return s_recording_context->record_callback(
        s_recording_context->entity, property_path, value, type);
}

bool AutoInspector::draw(const TypeInfo& type_info, void* instance, CommandHistory* /*history*/) {
    bool modified = false;

    for (const auto& prop : type_info.properties()) {
        // Check for hidden properties
        if (has_flag(prop.flags, PropertyFlags::Hidden)) {
            continue;
        }

        // Check for header flag
        if (has_flag(prop.flags, PropertyFlags::Header)) {
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Check for space flag
        if (has_flag(prop.flags, PropertyFlags::Space)) {
            ImGui::Spacing();
        }

        // Draw the property
        if (draw_property(prop, instance)) {
            modified = true;
        }

        // Show tooltip if available
        if (!prop.tooltip.empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", prop.tooltip.c_str());
        }
    }

    return modified;
}

bool AutoInspector::draw_property(const PropertyInfo& prop, void* instance) {
    // Handle read-only properties
    bool read_only = has_flag(prop.flags, PropertyFlags::ReadOnly);
    if (read_only) {
        ImGui::BeginDisabled();
    }

    bool modified = false;

    switch (prop.type) {
        case PropertyType::Bool:
            modified = draw_bool(prop, instance);
            break;
        case PropertyType::Int:
            modified = draw_int(prop, instance);
            break;
        case PropertyType::Float:
            modified = draw_float(prop, instance);
            break;
        case PropertyType::Double:
            modified = draw_double(prop, instance);
            break;
        case PropertyType::String:
            modified = draw_string(prop, instance);
            break;
        case PropertyType::Vec2:
            modified = draw_vec2(prop, instance);
            break;
        case PropertyType::Vec3:
            modified = draw_vec3(prop, instance);
            break;
        case PropertyType::Color:
            modified = draw_color(prop, instance);
            break;
        case PropertyType::Enum:
            modified = draw_enum(prop, instance);
            break;
        default:
            ImGui::Text("%s: (unknown type)", prop.display_name.c_str());
            break;
    }

    if (read_only) {
        ImGui::EndDisabled();
    }

    return modified;
}

bool AutoInspector::draw_bool(const PropertyInfo& prop, void* instance) {
    bool* value = prop.get_ptr<bool>(instance);
    PropertyGuard guard(prop, s_recording_context);

    bool modified = ImGui::Checkbox("##value", value);

    if (modified && s_recording_context) {
        try_record(prop.name, *value, engine::animation::PropertyValueType::Bool);
        modified = false;
    }

    return modified;
}

bool AutoInspector::draw_int(const PropertyInfo& prop, void* instance) {
    int* value = prop.get_ptr<int>(instance);
    PropertyGuard guard(prop, s_recording_context);

    bool modified = has_flag(prop.flags, PropertyFlags::Slider)
        ? ImGui::DragInt("##value", value, prop.step,
            static_cast<int>(prop.min_value), static_cast<int>(prop.max_value))
        : ImGui::DragInt("##value", value, prop.step);

    if (modified && s_recording_context) {
        try_record(prop.name, *value, engine::animation::PropertyValueType::Int);
        modified = false;
    }

    return modified;
}

bool AutoInspector::draw_float(const PropertyInfo& prop, void* instance) {
    float* value = prop.get_ptr<float>(instance);
    PropertyGuard guard(prop, s_recording_context);

    bool modified = false;
    if (has_flag(prop.flags, PropertyFlags::Angle)) {
        modified = ImGui::DragFloat("##value", value, 0.5f, -360.0f, 360.0f, "%.1f deg");
    } else if (has_flag(prop.flags, PropertyFlags::Slider)) {
        modified = ImGui::DragFloat("##value", value, prop.step, prop.min_value, prop.max_value);
    } else if (has_flag(prop.flags, PropertyFlags::Normalized)) {
        modified = ImGui::DragFloat("##value", value, 0.01f, 0.0f, 1.0f);
    } else if (has_flag(prop.flags, PropertyFlags::Percentage)) {
        float percent = *value * 100.0f;
        if (ImGui::DragFloat("##value", &percent, 0.5f, 0.0f, 100.0f, "%.1f%%")) {
            *value = percent / 100.0f;
            modified = true;
        }
    } else {
        modified = ImGui::DragFloat("##value", value, prop.step);
    }

    if (modified && s_recording_context) {
        try_record(prop.name, *value, engine::animation::PropertyValueType::Float);
        modified = false;
    }

    return modified;
}

bool AutoInspector::draw_double(const PropertyInfo& prop, void* instance) {
    double* value = prop.get_ptr<double>(instance);
    PropertyGuard guard(prop, s_recording_context);

    // ImGui doesn't have native double support, so we convert
    float temp = static_cast<float>(*value);
    bool modified = ImGui::DragFloat("##value", &temp, prop.step);
    if (modified) {
        *value = static_cast<double>(temp);
    }

    if (modified && s_recording_context) {
        try_record(prop.name, static_cast<float>(*value), engine::animation::PropertyValueType::Float);
        modified = false;
    }

    return modified;
}

bool AutoInspector::draw_string(const PropertyInfo& prop, void* instance) {
    std::string* value = prop.get_ptr<std::string>(instance);
    PropertyGuard guard(prop, s_recording_context);

    bool modified = has_flag(prop.flags, PropertyFlags::Multiline)
        ? InputTextMultiline("##value", value)
        : InputText("##value", value);

    if (modified && s_recording_context) {
        try_record(prop.name, *value, engine::animation::PropertyValueType::String);
        modified = false;
    }

    return modified;
}

bool AutoInspector::draw_vec2(const PropertyInfo& prop, void* instance) {
    float* value = prop.get_ptr<float>(instance);
    PropertyGuard guard(prop, s_recording_context);

    bool modified = ImGui::DragFloat2("##value", value, prop.step);

    if (modified && s_recording_context) {
        engine::animation::Vec2 vec_val{value[0], value[1]};
        try_record(prop.name, vec_val, engine::animation::PropertyValueType::Vec2);
        modified = false;
    }

    return modified;
}

bool AutoInspector::draw_vec3(const PropertyInfo& prop, void* instance) {
    float* value = prop.get_ptr<float>(instance);
    PropertyGuard guard(prop, s_recording_context);

    bool modified = ImGui::DragFloat3("##value", value, prop.step);

    if (modified && s_recording_context) {
        engine::animation::Vec3 vec_val{value[0], value[1], value[2]};
        try_record(prop.name, vec_val, engine::animation::PropertyValueType::Vec3);
        modified = false;
    }

    return modified;
}

bool AutoInspector::draw_color(const PropertyInfo& prop, void* instance) {
    float* value = prop.get_ptr<float>(instance);
    PropertyGuard guard(prop, s_recording_context);

    bool modified = ImGui::ColorEdit4("##value", value,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);

    if (modified && s_recording_context) {
        engine::animation::Vec4 vec_val{value[0], value[1], value[2], value[3]};
        try_record(prop.name, vec_val, engine::animation::PropertyValueType::Color);
        modified = false;
    }

    return modified;
}

bool AutoInspector::draw_enum(const PropertyInfo& prop, void* instance) {
    if (prop.enum_names.empty()) {
        ImGui::Text("%s: (no enum values)", prop.display_name.c_str());
        return false;
    }

    int* value = prop.get_ptr<int>(instance);
    PropertyGuard guard(prop, s_recording_context);

    const char* current = (*value >= 0 && *value < static_cast<int>(prop.enum_names.size()))
        ? prop.enum_names[*value].c_str()
        : "Unknown";

    bool modified = false;
    if (ImGui::BeginCombo("##value", current)) {
        for (int i = 0; i < static_cast<int>(prop.enum_names.size()); ++i) {
            bool is_selected = (*value == i);
            if (ImGui::Selectable(prop.enum_names[i].c_str(), is_selected)) {
                *value = i;
                modified = true;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return modified;
}

}

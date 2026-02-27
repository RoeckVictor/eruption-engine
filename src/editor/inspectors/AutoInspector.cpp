#include "AutoInspector.h"
#include "InspectorUtils.h"
#include <imgui.h>
#include <string>

using namespace engine::reflection;

namespace editor {

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
    ImGui::PushID(prop.name.c_str());

    ImGui::Text("%s", prop.display_name.c_str());
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);
    bool modified = ImGui::Checkbox("##value", value);

    ImGui::PopID();
    return modified;
}

bool AutoInspector::draw_int(const PropertyInfo& prop, void* instance) {
    int* value = prop.get_ptr<int>(instance);
    ImGui::PushID(prop.name.c_str());

    ImGui::Text("%s", prop.display_name.c_str());
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);

    bool modified = false;
    if (has_flag(prop.flags, PropertyFlags::Slider)) {
        modified = ImGui::DragInt("##value", value, prop.step,
            static_cast<int>(prop.min_value),
            static_cast<int>(prop.max_value));
    } else {
        modified = ImGui::DragInt("##value", value, prop.step);
    }

    ImGui::PopID();
    return modified;
}

bool AutoInspector::draw_float(const PropertyInfo& prop, void* instance) {
    float* value = prop.get_ptr<float>(instance);
    ImGui::PushID(prop.name.c_str());

    ImGui::Text("%s", prop.display_name.c_str());
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);

    bool modified = false;

    if (has_flag(prop.flags, PropertyFlags::Angle)) {
        // Display as angle with degree symbol
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

    ImGui::PopID();
    return modified;
}

bool AutoInspector::draw_double(const PropertyInfo& prop, void* instance) {
    double* value = prop.get_ptr<double>(instance);
    ImGui::PushID(prop.name.c_str());

    ImGui::Text("%s", prop.display_name.c_str());
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);

    // ImGui doesn't have native double support, so we convert
    float temp = static_cast<float>(*value);
    bool modified = ImGui::DragFloat("##value", &temp, prop.step);
    if (modified) {
        *value = static_cast<double>(temp);
    }

    ImGui::PopID();
    return modified;
}

bool AutoInspector::draw_string(const PropertyInfo& prop, void* instance) {
    std::string* value = prop.get_ptr<std::string>(instance);
    ImGui::PushID(prop.name.c_str());

    PropertyLabel(prop.display_name.c_str());

    bool modified = false;
    if (has_flag(prop.flags, PropertyFlags::Multiline)) {
        modified = InputTextMultiline("##value", value);
    } else {
        modified = InputText("##value", value);
    }

    ImGui::PopID();
    return modified;
}

bool AutoInspector::draw_vec2(const PropertyInfo& prop, void* instance) {
    float* value = prop.get_ptr<float>(instance);
    ImGui::PushID(prop.name.c_str());

    ImGui::Text("%s", prop.display_name.c_str());
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);

    bool modified = ImGui::DragFloat2("##value", value, prop.step);

    ImGui::PopID();
    return modified;
}

bool AutoInspector::draw_vec3(const PropertyInfo& prop, void* instance) {
    float* value = prop.get_ptr<float>(instance);
    ImGui::PushID(prop.name.c_str());

    ImGui::Text("%s", prop.display_name.c_str());
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);

    bool modified = ImGui::DragFloat3("##value", value, prop.step);

    ImGui::PopID();
    return modified;
}

bool AutoInspector::draw_color(const PropertyInfo& prop, void* instance) {
    float* value = prop.get_ptr<float>(instance);
    ImGui::PushID(prop.name.c_str());

    ImGui::Text("%s", prop.display_name.c_str());
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);

    // Assume RGBA (4 floats)
    bool modified = ImGui::ColorEdit4("##value", value,
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);

    ImGui::PopID();
    return modified;
}

bool AutoInspector::draw_enum(const PropertyInfo& prop, void* instance) {
    if (prop.enum_names.empty()) {
        ImGui::Text("%s: (no enum values)", prop.display_name.c_str());
        return false;
    }

    int* value = prop.get_ptr<int>(instance);
    ImGui::PushID(prop.name.c_str());

    ImGui::Text("%s", prop.display_name.c_str());
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(-1);

    // Get current enum name
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

    ImGui::PopID();
    return modified;
}

}

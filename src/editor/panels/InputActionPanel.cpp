#include "InputActionPanel.h"
#include "engine/core/Engine.h"
#include "engine/platform/Input.h"
#include "engine/platform/KeyCode.h"
#include "engine/platform/GamepadCodes.h"
#include <imgui.h>

namespace editor {

static const char* source_type_label(engine::platform::InputSourceType t) {
    switch (t) {
    case engine::platform::InputSourceType::Key:           return "Key";
    case engine::platform::InputSourceType::MouseButton:   return "Mouse";
    case engine::platform::InputSourceType::GamepadButton: return "Gamepad Btn";
    case engine::platform::InputSourceType::GamepadAxis:   return "Gamepad Axis";
    }
    return "?";
}

void InputActionPanel::draw(engine::platform::InputActionMap& action_map, engine::Engine& engine) {
    auto& actions = action_map.actions();

    if (ImGui::Button("+ Add Action")) {
        engine::platform::InputAction action;
        action.name = "NewAction";
        action_map.add_action(std::move(action));
    }
    ImGui::SameLine();
    if (ImGui::Button("Save") && !m_file_path.empty()) {
        action_map.save(m_file_path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load") && !m_file_path.empty()) {
        action_map.load(m_file_path);
    }

    ImGui::Separator();

    int remove_index = -1;
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
        auto& action = actions[i];
        ImGui::PushID(i);

        bool open = ImGui::TreeNodeEx(action.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        if (ImGui::SmallButton("X")) {
            remove_index = i;
        }

        if (open) {
            // Name
            char name_buf[128];
            snprintf(name_buf, sizeof(name_buf), "%s", action.name.c_str());
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                action.name = name_buf;
            }

            // Type
            const char* types[] = { "Button", "Axis" };
            int type_idx = static_cast<int>(action.type);
            if (ImGui::Combo("Type", &type_idx, types, 2)) {
                action.type = static_cast<engine::platform::ActionType>(type_idx);
            }

            // Bindings
            ImGui::Text("Bindings:");
            draw_bindings_list("##bindings", action.bindings, engine);

            if (action.type == engine::platform::ActionType::Axis) {
                ImGui::Text("Negative Bindings (-> -1):");
                draw_bindings_list("##neg", action.negative_bindings, engine);
                ImGui::Text("Positive Bindings (-> +1):");
                draw_bindings_list("##pos", action.positive_bindings, engine);
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (remove_index >= 0 && remove_index < static_cast<int>(actions.size())) {
        action_map.remove_action(actions[remove_index].name);
    }
}

void InputActionPanel::draw_bindings_list(const char* label,
                                           std::vector<engine::platform::InputBinding>& bindings,
                                           engine::Engine& engine) {
    ImGui::PushID(label);
    for (int i = 0; i < static_cast<int>(bindings.size()); ++i) {
        ImGui::PushID(i);
        draw_binding_row(bindings[i], i, engine);
        ImGui::SameLine();
        if (ImGui::SmallButton("-")) {
            bindings.erase(bindings.begin() + i);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    if (ImGui::SmallButton("+")) {
        bindings.push_back({});
    }
    ImGui::PopID();
}

void InputActionPanel::draw_binding_row(engine::platform::InputBinding& binding, int /*index*/,
                                         engine::Engine& /*engine*/) {
    const char* source_types[] = { "Key", "Mouse", "Gamepad Btn", "Gamepad Axis" };
    int src = static_cast<int>(binding.source_type);
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("##src", &src, source_types, 4)) {
        binding.source_type = static_cast<engine::platform::InputSourceType>(src);
        binding.code = 0;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::DragInt("##code", &binding.code, 1.0f, 0, 200);

    if (binding.source_type == engine::platform::InputSourceType::GamepadButton ||
        binding.source_type == engine::platform::InputSourceType::GamepadAxis) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(40);
        ImGui::DragInt("##gp", &binding.gamepad_index, 1.0f, 0, 3);
    }

    ImGui::SameLine();
    ImGui::Text("[%s:%d]", source_type_label(binding.source_type), binding.code);
}

}

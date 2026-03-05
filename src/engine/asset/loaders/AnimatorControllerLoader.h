#pragma once

#include "engine/asset/AssetLoader.h"
#include "engine/asset/VFS.h"
#include "engine/animation/AnimatorController.h"
#include "engine/core/Log.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <fstream>

namespace engine::asset {

// AnimatorController loader specialization
template<>
struct AssetLoader<animation::AnimatorController> {
    static std::unique_ptr<animation::AnimatorController> load(const VFS& vfs, const std::string& virtual_path) {
        auto file_data = vfs.read_file(virtual_path);
        if (file_data.is_err()) {
            ENGINE_ERR("AnimatorControllerLoader: Cannot read '%s': %s",
                       virtual_path.c_str(), file_data.error().message.c_str());
            return nullptr;
        }

        try {
            nlohmann::json j = nlohmann::json::parse(file_data.value().bytes);
            auto controller = std::make_unique<animation::AnimatorController>();
            parse_controller(j, *controller);

            ENGINE_LOG("AnimatorControllerLoader: Loaded '%s' (%zu states, %zu transitions)",
                       virtual_path.c_str(), controller->states.size(), controller->transitions.size());
            return controller;
        } catch (const nlohmann::json::exception& e) {
            ENGINE_ERR("AnimatorControllerLoader: JSON parse error in '%s': %s",
                       virtual_path.c_str(), e.what());
            return nullptr;
        }
    }

    static bool reload(animation::AnimatorController& controller, const VFS& vfs,
                      const std::string& virtual_path) {
        auto file_data = vfs.read_file(virtual_path);
        if (file_data.is_err()) return false;

        try {
            nlohmann::json j = nlohmann::json::parse(file_data.value().bytes);

            // Clear existing data
            controller.parameters.clear();
            controller.states.clear();
            controller.transitions.clear();
            controller.default_state.clear();

            parse_controller(j, controller);
            return true;
        } catch (const nlohmann::json::exception&) {
            return false;
        }
    }

private:
    static void parse_controller(const nlohmann::json& j, animation::AnimatorController& controller) {
        if (j.contains("name")) controller.name = j["name"].get<std::string>();
        if (j.contains("default_state")) controller.default_state = j["default_state"].get<std::string>();

        // Parse parameters
        if (j.contains("parameters") && j["parameters"].is_array()) {
            for (const auto& param_json : j["parameters"]) {
                animation::AnimatorParameter param;
                if (param_json.contains("name")) param.name = param_json["name"].get<std::string>();
                if (param_json.contains("type")) {
                    param.type = animation::parameter_type_from_string(param_json["type"].get<std::string>());
                }
                if (param_json.contains("default")) {
                    switch (param.type) {
                        case animation::ParameterType::Bool:
                        case animation::ParameterType::Trigger:
                            param.default_bool = param_json["default"].get<bool>();
                            break;
                        case animation::ParameterType::Int:
                            param.default_int = param_json["default"].get<int>();
                            break;
                        case animation::ParameterType::Float:
                            param.default_float = param_json["default"].get<float>();
                            break;
                    }
                }
                controller.parameters.push_back(std::move(param));
            }
        }

        // Parse states
        if (j.contains("states") && j["states"].is_array()) {
            for (const auto& state_json : j["states"]) {
                animation::AnimatorState state;
                if (state_json.contains("name")) state.name = state_json["name"].get<std::string>();
                if (state_json.contains("clip")) state.clip_path = state_json["clip"].get<std::string>();
                if (state_json.contains("speed")) state.speed = state_json["speed"].get<float>();
                if (state_json.contains("position") && state_json["position"].is_array() &&
                    state_json["position"].size() >= 2) {
                    state.editor_position.x = state_json["position"][0].get<float>();
                    state.editor_position.y = state_json["position"][1].get<float>();
                }
                controller.states.push_back(std::move(state));
            }
        }

        // Parse transitions
        if (j.contains("transitions") && j["transitions"].is_array()) {
            for (const auto& trans_json : j["transitions"]) {
                animation::StateTransition transition;
                if (trans_json.contains("from")) transition.from_state = trans_json["from"].get<std::string>();
                if (trans_json.contains("to")) transition.to_state = trans_json["to"].get<std::string>();
                if (trans_json.contains("blend_duration")) transition.blend_duration = trans_json["blend_duration"].get<float>();
                if (trans_json.contains("has_exit_time")) transition.has_exit_time = trans_json["has_exit_time"].get<bool>();
                if (trans_json.contains("exit_time")) transition.exit_time = trans_json["exit_time"].get<float>();

                // Parse conditions
                if (trans_json.contains("conditions") && trans_json["conditions"].is_array()) {
                    for (const auto& cond_json : trans_json["conditions"]) {
                        animation::TransitionCondition condition;
                        if (cond_json.contains("parameter")) {
                            condition.parameter_name = cond_json["parameter"].get<std::string>();
                        }
                        if (cond_json.contains("op")) {
                            condition.op = animation::compare_op_from_string(cond_json["op"].get<std::string>());
                        }
                        if (cond_json.contains("value")) {
                            const auto& v = cond_json["value"];

                            // Look up the parameter type to ensure we store the value
                            // with the correct type (JSON doesn't distinguish int from float)
                            const animation::AnimatorParameter* param = nullptr;
                            for (const auto& p : controller.parameters) {
                                if (p.name == condition.parameter_name) {
                                    param = &p;
                                    break;
                                }
                            }

                            if (param) {
                                // Load value based on parameter type, not JSON type
                                switch (param->type) {
                                    case animation::ParameterType::Bool:
                                    case animation::ParameterType::Trigger:
                                        condition.value = v.get<bool>();
                                        break;
                                    case animation::ParameterType::Int:
                                        condition.value = v.get<int>();
                                        break;
                                    case animation::ParameterType::Float:
                                        // Force float conversion even if JSON has integer
                                        condition.value = v.get<float>();
                                        break;
                                }
                            } else {
                                // Fallback: use JSON type detection
                                if (v.is_boolean()) {
                                    condition.value = v.get<bool>();
                                } else if (v.is_number()) {
                                    // Default to float for numbers if param not found
                                    condition.value = v.get<float>();
                                }
                            }
                        }
                        transition.conditions.push_back(std::move(condition));
                    }
                }
                controller.transitions.push_back(std::move(transition));
            }
        }
    }
};

}

// Serialization helpers for saving controllers
namespace engine::animation {

inline void to_json(nlohmann::json& j, const AnimatorController& controller) {
    j["name"] = controller.name;
    j["default_state"] = controller.default_state;

    // Serialize parameters
    j["parameters"] = nlohmann::json::array();
    for (const auto& param : controller.parameters) {
        nlohmann::json param_json;
        param_json["name"] = param.name;
        param_json["type"] = parameter_type_to_string(param.type);
        switch (param.type) {
            case ParameterType::Bool:
            case ParameterType::Trigger:
                param_json["default"] = param.default_bool;
                break;
            case ParameterType::Int:
                param_json["default"] = param.default_int;
                break;
            case ParameterType::Float:
                param_json["default"] = param.default_float;
                break;
        }
        j["parameters"].push_back(param_json);
    }

    // Serialize states
    j["states"] = nlohmann::json::array();
    for (const auto& state : controller.states) {
        nlohmann::json state_json;
        state_json["name"] = state.name;
        state_json["clip"] = state.clip_path;
        state_json["speed"] = state.speed;
        state_json["position"] = {state.editor_position.x, state.editor_position.y};
        j["states"].push_back(state_json);
    }

    // Serialize transitions
    j["transitions"] = nlohmann::json::array();
    for (const auto& trans : controller.transitions) {
        nlohmann::json trans_json;
        trans_json["from"] = trans.from_state;
        trans_json["to"] = trans.to_state;
        trans_json["blend_duration"] = trans.blend_duration;
        if (trans.has_exit_time) {
            trans_json["has_exit_time"] = true;
            trans_json["exit_time"] = trans.exit_time;
        }

        trans_json["conditions"] = nlohmann::json::array();
        for (const auto& cond : trans.conditions) {
            nlohmann::json cond_json;
            cond_json["parameter"] = cond.parameter_name;
            cond_json["op"] = compare_op_to_string(cond.op);

            // Serialize value based on variant type
            std::visit([&](const auto& val) {
                cond_json["value"] = val;
            }, cond.value);

            trans_json["conditions"].push_back(cond_json);
        }
        j["transitions"].push_back(trans_json);
    }
}

inline bool save_controller(const AnimatorController& controller, const std::string& path) {
    try {
        nlohmann::json j;
        to_json(j, controller);

        std::ofstream file(path);
        if (!file.is_open()) return false;

        file << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

}

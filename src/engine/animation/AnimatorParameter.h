#pragma once

#include <string>

namespace engine::animation {

enum class ParameterType {
    Bool,
    Int,
    Float,
    Trigger
};

inline const char* parameter_type_to_string(ParameterType type) {
    switch (type) {
        case ParameterType::Bool:    return "bool";
        case ParameterType::Int:     return "int";
        case ParameterType::Float:   return "float";
        case ParameterType::Trigger: return "trigger";
        default:                     return "float";
    }
}

inline ParameterType parameter_type_from_string(const std::string& str) {
    if (str == "bool")    return ParameterType::Bool;
    if (str == "int")     return ParameterType::Int;
    if (str == "float")   return ParameterType::Float;
    if (str == "trigger") return ParameterType::Trigger;
    return ParameterType::Float;
}

struct AnimatorParameter {
    std::string name;
    ParameterType type = ParameterType::Float;

    bool default_bool = false;
    int default_int = 0;
    float default_float = 0.0f;

    AnimatorParameter() = default;
    AnimatorParameter(std::string n, ParameterType t)
        : name(std::move(n)), type(t) {}

    static AnimatorParameter make_bool(const std::string& name, bool default_value = false) {
        AnimatorParameter p(name, ParameterType::Bool);
        p.default_bool = default_value;
        return p;
    }

    static AnimatorParameter make_int(const std::string& name, int default_value = 0) {
        AnimatorParameter p(name, ParameterType::Int);
        p.default_int = default_value;
        return p;
    }

    static AnimatorParameter make_float(const std::string& name, float default_value = 0.0f) {
        AnimatorParameter p(name, ParameterType::Float);
        p.default_float = default_value;
        return p;
    }

    static AnimatorParameter make_trigger(const std::string& name) {
        return AnimatorParameter(name, ParameterType::Trigger);
    }
};

}

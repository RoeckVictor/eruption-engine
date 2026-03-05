#pragma once

#include "engine/core/MathConstants.h"
#include <string>
#include <variant>
#include <cmath>

namespace engine::animation {

enum class CompareOp {
    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual
};

inline const char* compare_op_to_string(CompareOp op) {
    switch (op) {
        case CompareOp::Equal:        return "equal";
        case CompareOp::NotEqual:     return "not_equal";
        case CompareOp::Greater:      return "greater";
        case CompareOp::Less:         return "less";
        case CompareOp::GreaterEqual: return "greater_equal";
        case CompareOp::LessEqual:    return "less_equal";
        default:                      return "equal";
    }
}

inline CompareOp compare_op_from_string(const std::string& str) {
    if (str == "equal")         return CompareOp::Equal;
    if (str == "not_equal")     return CompareOp::NotEqual;
    if (str == "greater")       return CompareOp::Greater;
    if (str == "less")          return CompareOp::Less;
    if (str == "greater_equal") return CompareOp::GreaterEqual;
    if (str == "less_equal")    return CompareOp::LessEqual;
    return CompareOp::Equal;
}

struct TransitionCondition {
    std::string parameter_name;
    CompareOp op = CompareOp::Equal;

    std::variant<bool, int, float> value;

    TransitionCondition() : value(false) {}
    TransitionCondition(std::string param, CompareOp compare_op, bool val)
        : parameter_name(std::move(param)), op(compare_op), value(val) {}
    TransitionCondition(std::string param, CompareOp compare_op, int val)
        : parameter_name(std::move(param)), op(compare_op), value(val) {}
    TransitionCondition(std::string param, CompareOp compare_op, float val)
        : parameter_name(std::move(param)), op(compare_op), value(val) {}

    bool evaluate(bool param_value) const {
        if (!std::holds_alternative<bool>(value)) return false;
        bool compare_value = std::get<bool>(value);

        switch (op) {
            case CompareOp::Equal:    return param_value == compare_value;
            case CompareOp::NotEqual: return param_value != compare_value;
            default:                  return param_value == compare_value;
        }
    }

    bool evaluate(int param_value) const {
        if (!std::holds_alternative<int>(value)) return false;
        int compare_value = std::get<int>(value);

        switch (op) {
            case CompareOp::Equal:        return param_value == compare_value;
            case CompareOp::NotEqual:     return param_value != compare_value;
            case CompareOp::Greater:      return param_value > compare_value;
            case CompareOp::Less:         return param_value < compare_value;
            case CompareOp::GreaterEqual: return param_value >= compare_value;
            case CompareOp::LessEqual:    return param_value <= compare_value;
            default:                      return param_value == compare_value;
        }
    }

    bool evaluate(float param_value) const {
        if (!std::holds_alternative<float>(value)) return false;
        float compare_value = std::get<float>(value);

        switch (op) {
            case CompareOp::Equal:        return std::abs(param_value - compare_value) < engine::EPSILON;
            case CompareOp::NotEqual:     return std::abs(param_value - compare_value) >= engine::EPSILON;
            case CompareOp::Greater:      return param_value > compare_value;
            case CompareOp::Less:         return param_value < compare_value;
            case CompareOp::GreaterEqual: return param_value >= compare_value;
            case CompareOp::LessEqual:    return param_value <= compare_value;
            default:                      return std::abs(param_value - compare_value) < engine::EPSILON;
        }
    }

    static TransitionCondition bool_equals(const std::string& param, bool val) {
        return TransitionCondition(param, CompareOp::Equal, val);
    }

    static TransitionCondition float_greater(const std::string& param, float val) {
        return TransitionCondition(param, CompareOp::Greater, val);
    }

    static TransitionCondition float_less(const std::string& param, float val) {
        return TransitionCondition(param, CompareOp::Less, val);
    }

    static TransitionCondition trigger(const std::string& param) {
        return TransitionCondition(param, CompareOp::Equal, true);
    }
};

}

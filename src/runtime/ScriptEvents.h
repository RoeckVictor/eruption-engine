#pragma once

#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <variant>
#include <cstdint>

namespace runtime {

struct CollisionInfo {
    entt::entity other_entity = entt::null;
    float normal_x = 0.0f;
    float normal_y = 0.0f;
    float point_x = 0.0f;
    float point_y = 0.0f;
    float impulse = 0.0f;
};

using EventHandle = uint32_t;
using EventValue = std::variant<int, float, bool, std::string, entt::entity>;

struct EventData {
    std::unordered_map<std::string, EventValue> values;

    // Set a value in the event data
    template<typename T>
    void set(const std::string& key, T value) {
        values[key] = EventValue(std::move(value));
    }

    // Get a value from the event data, with a default if not found or wrong type
    template<typename T>
    T get(const std::string& key, T default_value = T{}) const {
        auto it = values.find(key);
        if (it == values.end()) return default_value;
        if (auto* ptr = std::get_if<T>(&it->second)) {
            return *ptr;
        }
        return default_value;
    }

    // Check if a key exists
    bool has(const std::string& key) const {
        return values.find(key) != values.end();
    }

    // Clear all values
    void clear() {
        values.clear();
    }
};

struct ContactPair {
    entt::entity entity_a = entt::null;
    entt::entity entity_b = entt::null;
    bool is_trigger = false;

    bool operator==(const ContactPair& other) const {
        return (entity_a == other.entity_a && entity_b == other.entity_b) ||
               (entity_a == other.entity_b && entity_b == other.entity_a);
    }
};

// Hash function for ContactPair to use in unordered containers
struct ContactPairHash {
    std::size_t operator()(const ContactPair& pair) const {
        // Order-independent hash
        auto a = static_cast<std::size_t>(entt::to_integral(pair.entity_a));
        auto b = static_cast<std::size_t>(entt::to_integral(pair.entity_b));
        if (a > b) std::swap(a, b);
        return a ^ (b << 1);
    }
};

}

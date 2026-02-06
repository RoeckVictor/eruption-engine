#pragma once

#include "engine/reflection/TypeInfo.h"
#include "engine/reflection/PropertyInfo.h"

namespace editor {

class CommandHistory;

/// Automatically generates ImGui inspector widgets from reflection data.
class AutoInspector {
public:
    /// Draw inspector UI for a reflected type.
    /// Returns true if any value was modified.
    static bool draw(const engine::reflection::TypeInfo& type_info, void* instance,
                     CommandHistory* history = nullptr);

    /// Draw a single property.
    /// Returns true if the value was modified.
    static bool draw_property(const engine::reflection::PropertyInfo& prop, void* instance);

private:
    // Individual property type handlers
    static bool draw_bool(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_int(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_float(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_double(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_string(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_vec2(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_vec3(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_color(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_enum(const engine::reflection::PropertyInfo& prop, void* instance);
};

} // namespace editor

#pragma once

#include "RecordingContext.h"
#include "engine/reflection/TypeInfo.h"
#include "engine/reflection/PropertyInfo.h"
#include <string>

namespace editor {

class CommandHistory;

// Automatically generates ImGui inspector widgets from reflection data
class AutoInspector {
public:
    static void set_recording_context(const RecordingContext* ctx);

    static bool draw(const engine::reflection::TypeInfo& type_info, void* instance,
                     CommandHistory* history = nullptr);

    static bool draw_property(const engine::reflection::PropertyInfo& prop, void* instance);

private:
    static bool draw_bool(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_int(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_float(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_double(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_string(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_vec2(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_vec3(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_color(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_enum(const engine::reflection::PropertyInfo& prop, void* instance);
    static bool draw_entity_ref(const engine::reflection::PropertyInfo& prop, void* instance);

    static bool try_record(const std::string& prop_name,
                           const engine::animation::PropertyValue& value,
                           engine::animation::PropertyValueType type);

    static const RecordingContext* s_recording_context;
};

}

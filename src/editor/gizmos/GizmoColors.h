#pragma once

#include <imgui.h>

namespace editor {
namespace gizmo_colors {

constexpr ImU32 X_AXIS = IM_COL32(220, 60, 60, 255);
constexpr ImU32 X_AXIS_HOVER = IM_COL32(255, 120, 120, 255);

constexpr ImU32 Y_AXIS = IM_COL32(60, 180, 60, 255);
constexpr ImU32 Y_AXIS_HOVER = IM_COL32(120, 255, 120, 255);

constexpr ImU32 XY_PLANE = IM_COL32(255, 220, 60, 200);
constexpr ImU32 XY_PLANE_HOVER = IM_COL32(255, 255, 120, 255);

constexpr ImU32 ROTATION = IM_COL32(60, 120, 220, 255);
constexpr ImU32 ROTATION_HOVER = IM_COL32(120, 180, 255, 255);

constexpr ImU32 UNIFORM_SCALE = IM_COL32(200, 200, 200, 255);
constexpr ImU32 UNIFORM_SCALE_HOVER = IM_COL32(255, 255, 255, 255);

constexpr ImU32 INDICATOR = IM_COL32(255, 255, 255, 200);
constexpr ImU32 TEXT = IM_COL32(255, 255, 255, 255);

}
}

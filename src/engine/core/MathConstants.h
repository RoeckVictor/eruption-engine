#pragma once

#include <numbers>

namespace engine {

inline constexpr float PI = std::numbers::pi_v<float>;
inline constexpr float DEG_TO_RAD = PI / 180.0f;
inline constexpr float RAD_TO_DEG = 180.0f / PI;

} // namespace engine

#pragma once

#include <chrono>

namespace engine::platform {

class Timer {
public:
    /// Initialize timer with optional max delta time and fixed timestep.
    /// @param max_dt Maximum frame delta time (clamps spiral-of-death)
    /// @param fixed_dt Fixed timestep for physics updates
    void init(double max_dt = 0.25, double fixed_dt = 1.0 / 60.0);
    void update();
    bool consume_fixed_step();

    double delta_time() const { return m_delta_time; }
    double fixed_dt() const { return m_fixed_dt; }
    void set_fixed_dt(double dt) { if (dt > 0.0) m_fixed_dt = dt; }
    int frame_count() const { return m_frame_count; }

    // Fraction of a fixed step remaining in the accumulator (0.0 to ~1.0).
    // Use for interpolating between physics states during rendering.
    float interpolation_alpha() const { return (float)(m_accumulator / m_fixed_dt); }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint m_last_time{};
    double m_delta_time = 0.0;
    double m_accumulator = 0.0;
    double m_fixed_dt = 1.0 / 60.0;
    double m_max_delta_time = 0.25;
    int m_frame_count = 0;
};

} // namespace engine::platform

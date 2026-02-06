#include "engine/platform/Timer.h"

namespace engine::platform {

void Timer::init(double max_dt, double fixed_dt) {
    m_max_delta_time = max_dt;
    m_fixed_dt = fixed_dt;
    m_last_time = Clock::now();
}

void Timer::update() {
    TimePoint now = Clock::now();
    std::chrono::duration<double> elapsed = now - m_last_time;
    m_delta_time = elapsed.count();
    if (m_delta_time > m_max_delta_time) m_delta_time = m_max_delta_time;
    m_last_time = now;
    m_accumulator += m_delta_time;
    m_frame_count++;
}

bool Timer::consume_fixed_step() {
    if (m_accumulator >= m_fixed_dt) {
        m_accumulator -= m_fixed_dt;
        return true;
    }
    return false;
}

} // namespace engine::platform

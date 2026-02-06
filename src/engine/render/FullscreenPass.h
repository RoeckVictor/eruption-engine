#pragma once
#include <cstdint>

namespace engine::render {

class FullscreenPass {
public:
    FullscreenPass() = default;
    ~FullscreenPass();

    FullscreenPass(const FullscreenPass&) = delete;
    FullscreenPass& operator=(const FullscreenPass&) = delete;
    FullscreenPass(FullscreenPass&& other) noexcept;
    FullscreenPass& operator=(FullscreenPass&& other) noexcept;

    bool init();
    void shutdown();
    void draw() const;

private:
    uint32_t m_vao = 0;
};

} // namespace engine::render

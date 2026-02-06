#include "engine/render/FullscreenPass.h"
#include <glad/gl.h>

namespace engine::render {

FullscreenPass::~FullscreenPass() {
    shutdown();
}

FullscreenPass::FullscreenPass(FullscreenPass&& other) noexcept
    : m_vao(other.m_vao)
{
    other.m_vao = 0;
}

FullscreenPass& FullscreenPass::operator=(FullscreenPass&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_vao = other.m_vao;
        other.m_vao = 0;
    }
    return *this;
}

bool FullscreenPass::init() {
    glGenVertexArrays(1, &m_vao);
    return m_vao != 0;
}

void FullscreenPass::shutdown() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}

void FullscreenPass::draw() const {
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

} // namespace engine::render

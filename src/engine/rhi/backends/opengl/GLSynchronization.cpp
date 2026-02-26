#include "GLSynchronization.h"
#include <glad/gl.h>

namespace engine::rhi {

GLFence::~GLFence() {
    destroy();
}

GLFence::GLFence(GLFence&& other) noexcept
    : m_sync(other.m_sync)
{
    other.m_sync = nullptr;
}

GLFence& GLFence::operator=(GLFence&& other) noexcept {
    if (this != &other) {
        destroy();
        m_sync = other.m_sync;
        other.m_sync = nullptr;
    }
    return *this;
}

bool GLFence::init() {
    // Fence is created lazily on insert()
    return true;
}

void GLFence::destroy() {
    if (m_sync) {
        glDeleteSync(static_cast<GLsync>(m_sync));
        m_sync = nullptr;
    }
}

void GLFence::insert() {
    // Delete existing fence if any
    if (m_sync) {
        glDeleteSync(static_cast<GLsync>(m_sync));
    }
    // Create a new fence at the current point in the command stream
    m_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool GLFence::wait(uint64_t timeout_ns) {
    if (!m_sync) {
        return true; // No fence to wait on
    }

    // Convert nanoseconds to OpenGL timeout (also in nanoseconds)
    GLuint64 gl_timeout = (timeout_ns == UINT64_MAX) ? GL_TIMEOUT_IGNORED : timeout_ns;

    GLenum result = glClientWaitSync(static_cast<GLsync>(m_sync),
                                      GL_SYNC_FLUSH_COMMANDS_BIT,
                                      gl_timeout);

    return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED;
}

void GLFence::reset() {
    // OpenGL sync objects can't be reset - destroy and recreate on next insert()
    destroy();
}

bool GLFence::is_signaled() const {
    if (!m_sync) {
        return true;
    }

    GLint status = GL_UNSIGNALED;
    GLsizei length = 0;
    glGetSynciv(static_cast<GLsync>(m_sync), GL_SYNC_STATUS, sizeof(status), &length, &status);

    return status == GL_SIGNALED;
}

bool GLEvent::init() {
    m_signaled = false;
    return true;
}

void GLEvent::set() {
    m_signaled = true;
}

void GLEvent::reset() {
    m_signaled = false;
}

bool GLEvent::is_set() const {
    return m_signaled;
}

bool GLSemaphore::init() {
    // No-op for OpenGL
    return true;
}

bool GLTimelineSemaphore::init() {
    m_value = 0;
    return true;
}

uint64_t GLTimelineSemaphore::value() const {
    return m_value;
}

void GLTimelineSemaphore::signal(uint64_t value) {
    // Simple CPU-side signaling since OpenGL doesn't support this
    m_value = value;
}

bool GLTimelineSemaphore::wait(uint64_t value, uint64_t /*timeout_ns*/) {
    // CPU-side wait - immediately returns since we can't actually wait on GPU
    return m_value >= value;
}

}

#pragma once

#include "RHITypes.h"

namespace engine::rhi {

// Abstract graphics/compute pipeline state object
// Encapsulates all state needed for rendering: shader, vertex layout, blend, etc.
class RHIPipeline {
public:
    virtual ~RHIPipeline() = default;

    RHIPipeline(const RHIPipeline&) = delete;
    RHIPipeline& operator=(const RHIPipeline&) = delete;

    virtual void bind() = 0;
    virtual void* native_handle() const = 0;

    RHIShader* shader() const { return m_shader; }

    bool valid() const { return m_valid; }
    bool is_compute() const { return m_is_compute; }

protected:
    RHIPipeline() = default;

    RHIShader* m_shader = nullptr;
    bool m_valid = false;
    bool m_is_compute = false;
};

}

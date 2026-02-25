#include "RHIDevice.h"
#include "RHIContext.h"
#include "backends/opengl/GLDevice.h"

namespace engine::rhi {

// Global device pointer (non-owning)
static RHIDevice* g_current_device = nullptr;

void set_current_device(RHIDevice* device) {
    g_current_device = device;
}

RHIDevice* get_current_device() {
    return g_current_device;
}

RHIContext* get_current_context() {
    return g_current_device ? g_current_device->context() : nullptr;
}

std::unique_ptr<RHIDevice> create_rhi_device(Backend backend, ProcAddressFunc proc_address) {
    switch (backend) {
        case Backend::OpenGL:
            if (!proc_address) {
                return nullptr; // OpenGL requires a proc address loader
            }
            return create_opengl_device(proc_address);

        case Backend::Vulkan:
        case Backend::D3D12:
        case Backend::Metal:
            // Not yet implemented
            return nullptr;

        default:
            return nullptr;
    }
}

} // namespace engine::rhi

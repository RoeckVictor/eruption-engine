#include "RHIDevice.h"
#include "RHIContext.h"
#include "engine/core/Log.h"
#include "backends/opengl/GLDevice.h"

#ifdef ERUPTION_VULKAN_SUPPORT
#include "backends/vulkan/VKDevice.h"
#endif

namespace engine::rhi {

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

std::unique_ptr<RHIDevice> create_rhi_device(Backend backend, const RHIDeviceCreateInfo& info) {
    switch (backend) {
        case Backend::OpenGL:
            if (!info.gl_proc_address) {
                return nullptr;
            }
            return create_opengl_device(info.gl_proc_address);

        case Backend::Vulkan:
#ifdef ERUPTION_VULKAN_SUPPORT
            if (!info.window_handle) {
                return nullptr;
            }
            return create_vulkan_device(info.window_handle);
#else
            ENGINE_ERR("Vulkan backend was requested but ERUPTION_VULKAN_SUPPORT is not enabled. "
                       "Ensure the Vulkan SDK is installed and CMake found it during configuration.");
            return nullptr;
#endif

        case Backend::D3D12:
        case Backend::Metal:
            // Not yet implemented
            return nullptr;

        default:
            return nullptr;
    }
}

}

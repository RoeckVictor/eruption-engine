#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHIPipeline.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace engine::rhi {

class VKDevice;
class VKShader;

class VKPipeline : public RHIPipeline {
public:
    VKPipeline() = default;
    ~VKPipeline() override;

    bool init(VKDevice* device, const PipelineDesc& desc);

    void bind() override;
    void* native_handle() const override { return m_pipeline; }

    VkPipelineLayout pipeline_layout() const { return m_pipeline_layout; }
    VkRenderPass render_pass() const { return m_render_pass; }

    // Recreate the VkPipeline if the shader was hot-reloaded.
    // Returns true if the pipeline was recreated or was already up-to-date.
    bool ensure_up_to_date();

    // Recreate the pipeline against a different render pass (e.g., when bound to a custom FBO)
    bool recreate_for_render_pass(VkRenderPass rp);

private:
    bool create_pipeline();

    VKDevice* m_device = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE; // owned by shader, not destroyed here
    VkRenderPass m_render_pass = VK_NULL_HANDLE;         // render pass this pipeline is compatible with

    PipelineDesc m_desc;                  // Stored for recreation on shader hot-reload
    std::vector<VertexAttribute> m_stored_attributes;  // Owned copies (PipelineDesc pointers may dangle)
    std::vector<VertexBinding> m_stored_bindings;
    uint32_t m_shader_reload_version = 0; // Tracks which shader version we were built against
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT

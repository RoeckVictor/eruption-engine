#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKPipeline.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include "VKShader.h"

namespace engine::rhi {

VKPipeline::~VKPipeline() {
    if (m_pipeline && m_device) {
        VkDevice dev = m_device->device();
        VkPipeline pipeline = m_pipeline;
        m_device->defer_deletion([dev, pipeline]() {
            vkDestroyPipeline(dev, pipeline, nullptr);
        });
        m_pipeline = VK_NULL_HANDLE;
    }
}

bool VKPipeline::init(VKDevice* device, const PipelineDesc& desc) {
    m_device = device;
    m_shader = desc.shader;
    m_desc = desc;

    // Store owned copies of vertex layout arrays so they survive after the caller's scope ends.
    // PipelineDesc::attributes and ::bindings are raw pointers that may dangle on hot-reload.
    if (desc.attributes && desc.attribute_count > 0) {
        m_stored_attributes.assign(desc.attributes, desc.attributes + desc.attribute_count);
        m_desc.attributes = m_stored_attributes.data();
    }
    if (desc.bindings && desc.binding_count > 0) {
        m_stored_bindings.assign(desc.bindings, desc.bindings + desc.binding_count);
        m_desc.bindings = m_stored_bindings.data();
    }

    auto* vk_shader = static_cast<VKShader*>(desc.shader);
    if (!vk_shader || !vk_shader->valid()) {
        ENGINE_ERR("VKPipeline: Invalid shader");
        return false;
    }

    m_pipeline_layout = vk_shader->pipeline_layout();
    m_is_compute = vk_shader->is_compute();
    m_shader_reload_version = vk_shader->reload_version();

    if (!create_pipeline()) return false;

    m_valid = true;
    return true;
}

bool VKPipeline::create_pipeline() {
    auto* vk_shader = static_cast<VKShader*>(m_shader);

    if (m_is_compute) {
        VkComputePipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = vk_shader->stage_infos()[0];
        pipeline_info.layout = m_pipeline_layout;

        if (!VK_CHECK(vkCreateComputePipelines(m_device->device(), VK_NULL_HANDLE, 1,
                                               &pipeline_info, nullptr, &m_pipeline))) {
            return false;
        }
    } else {
        // Vertex input
        std::vector<VkVertexInputBindingDescription> vk_bindings;
        std::vector<VkVertexInputAttributeDescription> vk_attributes;

        for (uint32_t i = 0; i < m_desc.binding_count; ++i) {
            VkVertexInputBindingDescription binding = {};
            binding.binding = m_desc.bindings[i].binding;
            binding.stride = m_desc.bindings[i].stride;
            binding.inputRate = m_desc.bindings[i].per_instance
                ? VK_VERTEX_INPUT_RATE_INSTANCE
                : VK_VERTEX_INPUT_RATE_VERTEX;
            vk_bindings.push_back(binding);
        }

        for (uint32_t i = 0; i < m_desc.attribute_count; ++i) {
            VkVertexInputAttributeDescription attr = {};
            attr.location = m_desc.attributes[i].location;
            attr.binding = m_desc.attributes[i].binding;
            attr.format = to_vk_vertex_format(m_desc.attributes[i].format);
            attr.offset = m_desc.attributes[i].offset;
            vk_attributes.push_back(attr);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input = {};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.vertexBindingDescriptionCount = static_cast<uint32_t>(vk_bindings.size());
        vertex_input.pVertexBindingDescriptions = vk_bindings.data();
        vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(vk_attributes.size());
        vertex_input.pVertexAttributeDescriptions = vk_attributes.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = to_vk_topology(m_desc.topology);
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = 2;
        dynamic_state.pDynamicStates = dynamic_states;

        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = m_desc.rasterizer.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = to_vk_cull_mode(m_desc.rasterizer.cull_mode);
        rasterizer.frontFace = to_vk_front_face(m_desc.rasterizer.front_face);
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = m_desc.depth_stencil.depth_test ? VK_TRUE : VK_FALSE;
        depth_stencil.depthWriteEnable = m_desc.depth_stencil.depth_write ? VK_TRUE : VK_FALSE;
        depth_stencil.depthCompareOp = to_vk_compare_op(m_desc.depth_stencil.depth_func);
        depth_stencil.stencilTestEnable = m_desc.depth_stencil.stencil_test ? VK_TRUE : VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = m_desc.blend.enabled ? VK_TRUE : VK_FALSE;
        if (m_desc.blend.enabled) {
            color_blend_attachment.srcColorBlendFactor = to_vk_blend_factor(m_desc.blend.src_color);
            color_blend_attachment.dstColorBlendFactor = to_vk_blend_factor(m_desc.blend.dst_color);
            color_blend_attachment.colorBlendOp = to_vk_blend_op(m_desc.blend.color_op);
            color_blend_attachment.srcAlphaBlendFactor = to_vk_blend_factor(m_desc.blend.src_alpha);
            color_blend_attachment.dstAlphaBlendFactor = to_vk_blend_factor(m_desc.blend.dst_alpha);
            color_blend_attachment.alphaBlendOp = to_vk_blend_op(m_desc.blend.alpha_op);
        }

        VkPipelineColorBlendStateCreateInfo color_blending = {};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = static_cast<uint32_t>(vk_shader->stage_infos().size());
        pipeline_info.pStages = vk_shader->stage_infos().data();
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = m_pipeline_layout;
        m_render_pass = m_desc.render_pass_handle
            ? static_cast<VkRenderPass>(m_desc.render_pass_handle)
            : m_device->swapchain_render_pass();
        pipeline_info.renderPass = m_render_pass;
        pipeline_info.subpass = 0;

        if (!VK_CHECK(vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1,
                                                &pipeline_info, nullptr, &m_pipeline))) {
            return false;
        }
    }
    return true;
}

bool VKPipeline::recreate_for_render_pass(VkRenderPass rp) {
    if (m_is_compute || rp == m_render_pass) return true;

    // Defer old pipeline destruction
    if (m_pipeline) {
        VkDevice dev = m_device->device();
        VkPipeline old_pipeline = m_pipeline;
        m_device->defer_deletion([dev, old_pipeline]() {
            vkDestroyPipeline(dev, old_pipeline, nullptr);
        });
        m_pipeline = VK_NULL_HANDLE;
    }

    // Persistently update the render pass handle so future recreations
    // (e.g., from ensure_up_to_date after hot-reload) use the correct one.
    m_desc.render_pass_handle = rp;
    bool ok = create_pipeline();

    if (!ok) {
        ENGINE_ERR("VKPipeline: Failed to recreate for different render pass");
        m_valid = false;
        return false;
    }
    return true;
}

bool VKPipeline::ensure_up_to_date() {
    auto* vk_shader = static_cast<VKShader*>(m_shader);
    if (!vk_shader || vk_shader->reload_version() == m_shader_reload_version) return true;

    // Shader was hot-reloaded — defer old pipeline destruction and recreate
    if (m_pipeline) {
        VkDevice dev = m_device->device();
        VkPipeline old_pipeline = m_pipeline;
        m_device->defer_deletion([dev, old_pipeline]() {
            vkDestroyPipeline(dev, old_pipeline, nullptr);
        });
        m_pipeline = VK_NULL_HANDLE;
    }

    m_pipeline_layout = vk_shader->pipeline_layout();
    m_shader_reload_version = vk_shader->reload_version();

    if (!create_pipeline()) {
        ENGINE_ERR("VKPipeline: Failed to recreate pipeline after shader hot-reload");
        m_valid = false;
        return false;
    }

    ENGINE_LOG("VKPipeline: Recreated after shader hot-reload");
    return true;
}

void VKPipeline::bind() {
    // Binding is handled by VKContext::bind_pipeline
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT

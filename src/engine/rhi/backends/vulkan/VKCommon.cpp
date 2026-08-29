#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKCommon.h"
#include "VKDevice.h"
#include "VKShader.h"
#include "VKTexture.h"

namespace engine::rhi {

bool VKDescriptorState::flush(VKDevice* device, VkCommandBuffer cmd,
                              VKShader* shader, VkPipelineLayout layout, bool is_compute) {
    if (!dirty || !cmd) return false;
    dirty = false;

    VkDescriptorSetLayout ds_layout = shader->descriptor_set_layout();
    if (ds_layout == VK_NULL_HANDLE) return false;

    const auto& reflected = shader->reflected_bindings();
    if (reflected.empty()) return false;

    VkDescriptorSet ds = device->allocate_frame_descriptor_set(ds_layout);
    if (ds == VK_NULL_HANDLE) return false;

    // Reuse pre-allocated vectors (clear keeps capacity)
    writes.clear();
    buffer_infos.clear();
    image_infos.clear();

    // Reserve up front so pointers into the vectors remain stable
    buffer_infos.reserve(reflected.size());
    image_infos.reserve(reflected.size());

    for (const auto& rb : reflected) {
        if (rb.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            uint32_t slot = rb.binding;
            if (slot >= MAX_SLOTS || ubos[slot] == VK_NULL_HANDLE) continue;

            auto& bi = buffer_infos.emplace_back();
            bi.buffer = ubos[slot];
            bi.offset = 0;
            bi.range = ubo_sizes[slot];

            VkWriteDescriptorSet w = {};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = ds;
            w.dstBinding = slot;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.pBufferInfo = &buffer_infos.back();
            writes.push_back(w);

        } else if (rb.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            uint32_t slot = rb.binding;
            if (slot >= MAX_SLOTS || ssbos[slot] == VK_NULL_HANDLE) continue;

            auto& bi = buffer_infos.emplace_back();
            bi.buffer = ssbos[slot];
            bi.offset = 0;
            bi.range = ssbo_sizes[slot];

            VkWriteDescriptorSet w = {};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = ds;
            w.dstBinding = slot;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.pBufferInfo = &buffer_infos.back();
            writes.push_back(w);

        } else if (rb.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
            // Use the reflected binding number to index into the bound images array.
            // This matches the user's bind_image(tex, unit) where unit = binding number.
            uint32_t slot = rb.binding;
            if (slot >= MAX_SLOTS || images[slot] == VK_NULL_HANDLE) continue;

            auto& ii = image_infos.emplace_back();
            ii.imageView = images[slot];
            ii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            ii.sampler = VK_NULL_HANDLE;

            VkWriteDescriptorSet w = {};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = ds;
            w.dstBinding = slot;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.pImageInfo = &image_infos.back();
            writes.push_back(w);

        } else if (rb.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            // Use the reflected binding number to index into the bound samplers array.
            // This matches the user's bind_texture(tex, unit) where unit = binding number.
            uint32_t slot = rb.binding;
            if (slot >= MAX_SLOTS || !samplers[slot]) continue;

            const VKTexture* tex = samplers[slot];
            auto& ii = image_infos.emplace_back();
            ii.imageView = tex->image_view();
            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ii.sampler = tex->sampler();

            VkWriteDescriptorSet w = {};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = ds;
            w.dstBinding = slot;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &image_infos.back();
            writes.push_back(w);
        }
    }

    if (!writes.empty()) {
        vkUpdateDescriptorSets(device->device(),
            static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        VkPipelineBindPoint bind_point = is_compute
            ? VK_PIPELINE_BIND_POINT_COMPUTE
            : VK_PIPELINE_BIND_POINT_GRAPHICS;

        vkCmdBindDescriptorSets(cmd, bind_point, layout, 0, 1, &ds, 0, nullptr);
        return true;
    }

    return false;
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT

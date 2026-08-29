#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKTexture.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include <vector>

namespace engine::rhi {

VKTexture::~VKTexture() {
    if (m_device) {
        VkDevice dev = m_device->device();
        VmaAllocator alloc = m_device->allocator();
        VkSampler sampler = m_sampler;
        VkImageView view = m_image_view;
        VkImage image = m_image;
        VmaAllocation allocation = m_allocation;

        if (sampler || view || (image && allocation)) {
            m_device->defer_deletion([dev, alloc, sampler, view, image, allocation]() {
                if (sampler)              vkDestroySampler(dev, sampler, nullptr);
                if (view)                 vkDestroyImageView(dev, view, nullptr);
                if (image && allocation)  vmaDestroyImage(alloc, image, allocation);
            });
        }

        m_sampler = VK_NULL_HANDLE;
        m_image_view = VK_NULL_HANDLE;
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

bool VKTexture::init(VKDevice* device, const TextureDesc& desc) {
    m_device = device;
    m_width = desc.width;
    m_height = desc.height;
    m_depth = desc.depth;
    m_dimension = desc.dimension;
    m_format = desc.format;
    m_vk_format = to_vk_format(desc.format);

    // Vulkan has poor support for 3-component formats (RGB8, RGB16F, RGB32F, etc.).
    // Most GPUs don't include them in the mandatory format support table.
    // Upgrade to 4-component and pad initial data if needed.
    bool format_upgraded = false;
    int original_channels = 0; // Track for data padding
    int original_bpc = 0;      // Bytes per channel

    if (m_vk_format == VK_FORMAT_R8G8B8_UNORM) {
        m_vk_format = VK_FORMAT_R8G8B8A8_UNORM; m_format = TextureFormat::RGBA8;
        format_upgraded = true; original_channels = 3; original_bpc = 1;
    } else if (m_vk_format == VK_FORMAT_R8G8B8_UINT) {
        m_vk_format = VK_FORMAT_R8G8B8A8_UINT; m_format = TextureFormat::RGBA8UI;
        format_upgraded = true; original_channels = 3; original_bpc = 1;
    } else if (m_vk_format == VK_FORMAT_R16G16B16_SFLOAT) {
        m_vk_format = VK_FORMAT_R16G16B16A16_SFLOAT; m_format = TextureFormat::RGBA16F;
        format_upgraded = true; original_channels = 3; original_bpc = 2;
    } else if (m_vk_format == VK_FORMAT_R32G32B32_SFLOAT) {
        m_vk_format = VK_FORMAT_R32G32B32A32_SFLOAT; m_format = TextureFormat::RGBA32F;
        format_upgraded = true; original_channels = 3; original_bpc = 4;
    }
    if (format_upgraded) {
        ENGINE_LOG_WARN("VKTexture: 3-component format not supported in Vulkan, upgrading to 4-component");
    }

    // Create image
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = to_vk_image_type(desc.dimension);
    image_info.format = m_vk_format;
    image_info.extent = {static_cast<uint32_t>(desc.width), static_cast<uint32_t>(desc.height),
                         static_cast<uint32_t>(desc.depth)};
    image_info.mipLevels = 1;
    image_info.arrayLayers = (desc.dimension == TextureDimension::TexCube) ? 6 : 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Determine usage flags from TextureDesc
    if (is_depth_format(desc.format)) {
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                         | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    } else {
        VkImageUsageFlags usage = 0;
        if (has_usage(desc.usage, TextureUsageFlags::Sampled))         usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (has_usage(desc.usage, TextureUsageFlags::Storage))         usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (has_usage(desc.usage, TextureUsageFlags::ColorAttachment)) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (has_usage(desc.usage, TextureUsageFlags::TransferSrc))     usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (has_usage(desc.usage, TextureUsageFlags::TransferDst))     usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.usage = usage;
    }

    if (desc.dimension == TextureDimension::TexCube) {
        image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (!VK_CHECK(vmaCreateImage(device->allocator(), &image_info, &alloc_info,
                                 &m_image, &m_allocation, nullptr))) {
        return false;
    }

    // Create image view
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = m_image;
    view_info.viewType = to_vk_image_view_type(desc.dimension);
    view_info.format = m_vk_format;
    view_info.subresourceRange.aspectMask = vk_aspect_from_format(m_vk_format);
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = image_info.arrayLayers;

    if (!VK_CHECK(vkCreateImageView(device->device(), &view_info, nullptr, &m_image_view))) {
        return false;
    }

    // Create sampler
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = to_vk_filter(desc.mag_filter);
    sampler_info.minFilter = to_vk_filter(desc.min_filter);
    sampler_info.addressModeU = to_vk_address_mode(desc.wrap_u);
    sampler_info.addressModeV = to_vk_address_mode(desc.wrap_v);
    sampler_info.addressModeW = to_vk_address_mode(desc.wrap_w);
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (!VK_CHECK(vkCreateSampler(device->device(), &sampler_info, nullptr, &m_sampler))) {
        return false;
    }

    // Upload initial data if provided
    if (desc.initial_data) {
        if (format_upgraded) {
            // Pad 3-channel → 4-channel (works for any bytes-per-channel)
            size_t pixel_count = static_cast<size_t>(desc.width) * desc.height * desc.depth;
            size_t src_stride = static_cast<size_t>(original_channels) * original_bpc;
            size_t dst_stride = 4u * original_bpc;
            std::vector<uint8_t> padded(pixel_count * dst_stride, 0);
            const uint8_t* src = static_cast<const uint8_t*>(desc.initial_data);
            for (size_t i = 0; i < pixel_count; ++i) {
                memcpy(&padded[i * dst_stride], &src[i * src_stride], src_stride);
                // For 8-bit formats, set alpha to 255; for float formats, leave as 0
                if (original_bpc == 1) {
                    padded[i * dst_stride + 3] = 255;
                }
            }
            upload(0, 0, desc.width, desc.height, padded.data());
        } else {
            upload(0, 0, desc.width, desc.height, desc.initial_data);
        }
    } else {
        // Transition to a usable layout even without data
        auto cmd = device->begin_single_time_commands();
        if (is_depth_format(desc.format)) {
            transition_layout(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        } else {
            transition_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        device->end_single_time_commands(cmd);
    }

    m_valid = true;
    return true;
}

void VKTexture::transition_layout(VkCommandBuffer cmd, VkImageLayout new_layout) const {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_current_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = vk_aspect_from_format(m_vk_format);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (m_current_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
        barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    } else if (m_current_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (m_current_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (m_current_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (m_current_layout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (m_current_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (m_current_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }

    if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dst_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if (new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (new_layout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    m_current_layout = new_layout;
}

void VKTexture::upload(int x, int y, int w, int h, const void* data) {
    if (!data || !m_device) return;

    size_t bpp = texture_format_bpp(m_format);
    size_t data_size = static_cast<size_t>(w) * h * bpp;

    // Create staging buffer
    VkBufferCreateInfo staging_info = {};
    staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_info.size = data_size;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo staging_alloc = {};
    staging_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    VmaAllocationInfo staging_result = {};
    if (vmaCreateBuffer(m_device->allocator(), &staging_info, &staging_alloc,
                        &staging_buffer, &staging_allocation, &staging_result) != VK_SUCCESS) {
        ENGINE_ERR("VKTexture::upload: Failed to create staging buffer");
        return;
    }

    memcpy(staging_result.pMappedData, data, data_size);
    vmaFlushAllocation(m_device->allocator(), staging_allocation, 0, data_size);

    auto cmd = m_device->begin_single_time_commands();

    transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {x, y, 0};
    region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};

    vkCmdCopyBufferToImage(cmd, staging_buffer, m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transition_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_device->end_single_time_commands(cmd);

    vmaDestroyBuffer(m_device->allocator(), staging_buffer, staging_allocation);
}

void VKTexture::readback(int x, int y, int w, int h,
                         void* dst, size_t dst_size) const {
    if (!dst || !m_device || !m_image) return;

    size_t bpp = texture_format_bpp(m_format);
    size_t copy_size = static_cast<size_t>(w) * h * bpp;
    if (copy_size > dst_size) copy_size = dst_size;

    VkBufferCreateInfo staging_info = {};
    staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_info.size = copy_size;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo staging_alloc = {};
    staging_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    VmaAllocationInfo staging_result = {};
    if (vmaCreateBuffer(m_device->allocator(), &staging_info, &staging_alloc,
                        &staging_buffer, &staging_allocation, &staging_result) != VK_SUCCESS) {
        ENGINE_ERR("VKTexture::readback: Failed to create staging buffer");
        return;
    }

    auto cmd = m_device->begin_single_time_commands();

    VkImageLayout old_layout = m_current_layout;
    if (m_current_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = vk_aspect_from_format(m_vk_format);
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {x, y, 0};
    region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};

    vkCmdCopyImageToBuffer(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        staging_buffer, 1, &region);

    if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        transition_layout(cmd, old_layout != VK_IMAGE_LAYOUT_UNDEFINED
            ? old_layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    m_device->end_single_time_commands(cmd);

    vmaInvalidateAllocation(m_device->allocator(), staging_allocation, 0, copy_size);
    memcpy(dst, staging_result.pMappedData, copy_size);

    vmaDestroyBuffer(m_device->allocator(), staging_buffer, staging_allocation);
}

void VKTexture::bind(uint32_t /*unit*/) const {
    // In Vulkan, textures are bound via descriptor sets
}

void VKTexture::bind_as_image(uint32_t /*unit*/, ImageAccess /*access*/) {
    // In Vulkan, storage images are bound via descriptor sets
}

void VKTexture::generate_mipmaps() {
    // Current implementation only supports 1 mip level — mipmaps not generated at creation.
    // Full mipmap generation would use vkCmdBlitImage to downsample each level.
    // For now, this is a valid no-op since all textures are created with mipLevels=1.
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT

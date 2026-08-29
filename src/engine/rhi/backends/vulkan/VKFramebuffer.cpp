#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKFramebuffer.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include "VKTexture.h"

namespace engine::rhi {

VKFramebuffer::~VKFramebuffer() {
    destroy();
}

void VKFramebuffer::destroy() {
    if (!m_device) return;
    VkDevice dev = m_device->device();

    // Defer GPU resource destruction until in-flight frames are done.
    // Use shared_ptr to hold the textures since std::function requires copyability.
    VkFramebuffer fb = m_framebuffer;
    VkFramebuffer fb_load = m_framebuffer_load;
    VkRenderPass rp = m_render_pass;
    VkRenderPass rp_load = m_render_pass_load;

    struct DeferredResources {
        std::vector<std::unique_ptr<VKTexture>> color_textures;
        std::unique_ptr<VKTexture> depth_texture;
    };
    auto resources = std::make_shared<DeferredResources>();
    resources->color_textures = std::move(m_owned_color_textures);
    resources->depth_texture = std::move(m_owned_depth_texture);

    if (fb || rp || fb_load || rp_load || !resources->color_textures.empty() || resources->depth_texture) {
        m_device->defer_deletion([dev, fb, fb_load, rp, rp_load, res = std::move(resources)]() {
            res->color_textures.clear();
            res->depth_texture.reset();
            if (fb)      vkDestroyFramebuffer(dev, fb, nullptr);
            if (fb_load) vkDestroyFramebuffer(dev, fb_load, nullptr);
            if (rp)      vkDestroyRenderPass(dev, rp, nullptr);
            if (rp_load) vkDestroyRenderPass(dev, rp_load, nullptr);
        });
    }

    m_framebuffer = VK_NULL_HANDLE;
    m_framebuffer_load = VK_NULL_HANDLE;
    m_render_pass = VK_NULL_HANDLE;
    m_render_pass_load = VK_NULL_HANDLE;
    m_color_attachments.clear();
    m_depth_attachment = nullptr;
}

// ---- Shared helpers ----

static bool create_render_pass_with_load_op(
    VkDevice device,
    const VkFormat* color_formats, uint32_t color_count,
    VkFormat depth_format, bool has_depth,
    VkAttachmentLoadOp load_op,
    VkRenderPass* out_render_pass)
{
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> color_refs;

    bool is_load = (load_op == VK_ATTACHMENT_LOAD_OP_LOAD);

    for (uint32_t i = 0; i < color_count; ++i) {
        VkAttachmentDescription color_attach = {};
        color_attach.format = color_formats[i];
        color_attach.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attach.loadOp = load_op;
        color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attach.initialLayout = is_load ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                             : VK_IMAGE_LAYOUT_UNDEFINED;
        color_attach.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments.push_back(color_attach);

        VkAttachmentReference ref = {};
        ref.attachment = i;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_refs.push_back(ref);
    }

    VkAttachmentReference depth_ref = {};
    if (has_depth) {
        VkAttachmentDescription depth_attach = {};
        depth_attach.format = depth_format;
        depth_attach.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attach.loadOp = load_op;
        depth_attach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attach.initialLayout = is_load ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                             : VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attach.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depth_attach);

        depth_ref.attachment = color_count;
        depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
    subpass.pColorAttachments = color_refs.data();
    subpass.pDepthStencilAttachment = has_depth ? &depth_ref : nullptr;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    if (is_load) {
        // Resuming after a barrier/transition — wait for prior writes
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                 | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                 | VK_ACCESS_SHADER_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                 | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                 | (has_depth ? (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                               | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) : 0u);
    } else {
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                 | (has_depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0u);
    }
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    rp_info.pAttachments = attachments.data();
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dependency;

    return VK_CHECK(vkCreateRenderPass(device, &rp_info, nullptr, out_render_pass));
}

bool VKFramebuffer::create_render_pass_internal(
    const VkFormat* color_formats, uint32_t color_count,
    VkFormat depth_format, bool has_depth)
{
    if (!create_render_pass_with_load_op(m_device->device(), color_formats, color_count,
                                         depth_format, has_depth,
                                         VK_ATTACHMENT_LOAD_OP_CLEAR, &m_render_pass)) {
        return false;
    }
    return create_render_pass_with_load_op(m_device->device(), color_formats, color_count,
                                            depth_format, has_depth,
                                            VK_ATTACHMENT_LOAD_OP_LOAD, &m_render_pass_load);
}

bool VKFramebuffer::create_framebuffer_internal(
    const VkImageView* views, uint32_t view_count,
    uint32_t width, uint32_t height)
{
    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = m_render_pass;
    fb_info.attachmentCount = view_count;
    fb_info.pAttachments = views;
    fb_info.width = width;
    fb_info.height = height;
    fb_info.layers = 1;

    return VK_CHECK(vkCreateFramebuffer(m_device->device(), &fb_info, nullptr, &m_framebuffer));
}

// ---- init_simple ----

bool VKFramebuffer::init_simple(VKDevice* device, int width, int height,
                                 TextureFormat color_format, bool create_depth) {
    m_device = device;
    m_width = width;
    m_height = height;
    m_color_format = color_format;
    m_depth_format = from_vk_depth_format(device->find_supported_depth_format());
    m_owns_textures = true;

    // Create color texture (needs color attachment + sampled for ImGui display + transfer for readback)
    TextureDesc color_desc;
    color_desc.width = width;
    color_desc.height = height;
    color_desc.format = color_format;
    color_desc.usage = TextureUsageFlags::ColorAttachment | TextureUsageFlags::Sampled
                     | TextureUsageFlags::TransferSrc | TextureUsageFlags::TransferDst;

    auto color_tex = std::make_unique<VKTexture>();
    if (!color_tex->init(device, color_desc)) return false;

    m_color_attachments.push_back(color_tex.get());
    m_owned_color_textures.push_back(std::move(color_tex));
    m_color_count = 1;

    // Create depth texture if requested
    if (create_depth) {
        TextureDesc depth_desc;
        depth_desc.width = width;
        depth_desc.height = height;
        depth_desc.format = m_depth_format;

        auto depth_tex = std::make_unique<VKTexture>();
        if (!depth_tex->init(device, depth_desc)) return false;

        m_depth_attachment = depth_tex.get();
        m_owned_depth_texture = std::move(depth_tex);
        m_has_depth = true;
    }

    // Create render pass and framebuffer using shared helpers
    VkFormat vk_color_fmt = to_vk_format(color_format);
    VkFormat vk_depth_fmt = to_vk_format(m_depth_format);
    if (!create_render_pass_internal(&vk_color_fmt, 1, vk_depth_fmt, create_depth)) return false;

    std::vector<VkImageView> views;
    for (auto* tex : m_color_attachments) {
        views.push_back(static_cast<VKTexture*>(tex)->image_view());
    }
    if (m_depth_attachment) {
        views.push_back(static_cast<VKTexture*>(m_depth_attachment)->image_view());
    }
    if (!create_framebuffer_internal(views.data(), static_cast<uint32_t>(views.size()),
                                     static_cast<uint32_t>(width), static_cast<uint32_t>(height))) {
        return false;
    }

    // Create framebuffer for the LOAD render pass (same views, different render pass)
    {
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = m_render_pass_load;
        fb_info.attachmentCount = static_cast<uint32_t>(views.size());
        fb_info.pAttachments = views.data();
        fb_info.width = static_cast<uint32_t>(width);
        fb_info.height = static_cast<uint32_t>(height);
        fb_info.layers = 1;
        if (!VK_CHECK(vkCreateFramebuffer(m_device->device(), &fb_info, nullptr, &m_framebuffer_load))) {
            return false;
        }
    }

    m_valid = true;
    return true;
}

// ---- init (external textures) ----

bool VKFramebuffer::init(VKDevice* device, const FramebufferDesc& desc) {
    m_device = device;
    m_width = desc.width;
    m_height = desc.height;
    m_owns_textures = false;

    // Collect external color attachments
    for (uint32_t i = 0; i < desc.color_attachment_count; ++i) {
        m_color_attachments.push_back(desc.color_attachments[i].texture);
    }
    m_color_count = desc.color_attachment_count;

    if (desc.depth_stencil_attachment.texture) {
        m_depth_attachment = desc.depth_stencil_attachment.texture;
        m_has_depth = true;
    }

    // Determine formats from attached textures
    if (!m_color_attachments.empty() && m_color_attachments[0]) {
        m_color_format = m_color_attachments[0]->format();
    }
    if (m_depth_attachment) {
        m_depth_format = m_depth_attachment->format();
    }

    // Build format arrays from the attached textures
    std::vector<VkFormat> color_formats;
    for (uint32_t i = 0; i < desc.color_attachment_count; ++i) {
        color_formats.push_back(to_vk_format(m_color_attachments[i]->format()));
    }
    VkFormat vk_depth_fmt = m_has_depth ? to_vk_format(m_depth_format) : VK_FORMAT_UNDEFINED;

    if (!create_render_pass_internal(color_formats.data(), static_cast<uint32_t>(color_formats.size()),
                                     vk_depth_fmt, m_has_depth)) {
        return false;
    }

    // Build view array
    std::vector<VkImageView> views;
    for (auto* tex : m_color_attachments) {
        views.push_back(static_cast<VKTexture*>(tex)->image_view());
    }
    if (m_depth_attachment) {
        views.push_back(static_cast<VKTexture*>(m_depth_attachment)->image_view());
    }
    if (!create_framebuffer_internal(views.data(), static_cast<uint32_t>(views.size()),
                                     static_cast<uint32_t>(desc.width), static_cast<uint32_t>(desc.height))) {
        return false;
    }

    // Create framebuffer for the LOAD render pass
    {
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = m_render_pass_load;
        fb_info.attachmentCount = static_cast<uint32_t>(views.size());
        fb_info.pAttachments = views.data();
        fb_info.width = static_cast<uint32_t>(desc.width);
        fb_info.height = static_cast<uint32_t>(desc.height);
        fb_info.layers = 1;
        if (!VK_CHECK(vkCreateFramebuffer(m_device->device(), &fb_info, nullptr, &m_framebuffer_load))) {
            return false;
        }
    }

    m_valid = true;
    return true;
}

void VKFramebuffer::begin_render_pass(VkCommandBuffer cmd, float r, float g, float b, float a,
                                      float depth, int stencil) {
    VkClearValue clear_values[2] = {};
    clear_values[0].color = {{r, g, b, a}};
    clear_values[1].depthStencil = {depth, static_cast<uint32_t>(stencil)};

    VkRenderPassBeginInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = m_render_pass;
    rp_info.framebuffer = m_framebuffer;
    rp_info.renderArea.offset = {0, 0};
    rp_info.renderArea.extent = {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height)};
    rp_info.clearValueCount = m_has_depth ? 2u : 1u;
    rp_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VKFramebuffer::begin_render_pass_load(VkCommandBuffer cmd) {
    // LOAD_OP_LOAD ignores clear values, but Vulkan still requires the array
    VkClearValue clear_values[2] = {};

    VkRenderPassBeginInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = m_render_pass_load;
    rp_info.framebuffer = m_framebuffer_load;
    rp_info.renderArea.offset = {0, 0};
    rp_info.renderArea.extent = {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height)};
    rp_info.clearValueCount = m_has_depth ? 2u : 1u;
    rp_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VKFramebuffer::sync_attachment_layouts() {
    // Color attachments transition to SHADER_READ_ONLY_OPTIMAL (our render pass finalLayout)
    for (auto* tex : m_color_attachments) {
        if (tex) {
            static_cast<VKTexture*>(tex)->override_tracked_layout(
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }
    // Depth attachment stays at DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    if (m_depth_attachment) {
        static_cast<VKTexture*>(m_depth_attachment)->override_tracked_layout(
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }
}

void VKFramebuffer::bind() {
    // Handled by VKContext::bind_framebuffer
}

void VKFramebuffer::unbind() {
    // Handled by VKContext
}

bool VKFramebuffer::resize(int width, int height) {
    if (!m_device || !m_owns_textures) return false;

    destroy();
    m_width = width;
    m_height = height;
    return init_simple(m_device, width, height, m_color_format, m_has_depth);
}

RHITexture* VKFramebuffer::color_attachment(uint32_t index) {
    return index < m_color_attachments.size() ? m_color_attachments[index] : nullptr;
}

RHITexture* VKFramebuffer::depth_stencil_attachment() {
    return m_depth_attachment;
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT

#pragma once

#include "engine/rhi/RHIDescriptorSet.h"
#include <vector>
#include <variant>

namespace engine::rhi {

// Simply stores the binding descriptions
class GLDescriptorSetLayout : public RHIDescriptorSetLayout {
public:
    GLDescriptorSetLayout() = default;
    ~GLDescriptorSetLayout() override = default;

    bool init(const DescriptorSetLayoutDesc& desc);

    const std::vector<DescriptorBinding>& bindings() const override { return m_bindings; }

private:
    std::vector<DescriptorBinding> m_bindings;
};

// Internal storage for a bound resource
struct GLBoundResource {
    DescriptorType type = DescriptorType::UniformBuffer;

    RHIBuffer* buffer = nullptr;
    size_t buffer_offset = 0;
    size_t buffer_range = 0;

    RHITexture* texture = nullptr;
    uint32_t mip_level = 0;
    BufferAccess access = BufferAccess::ReadWrite;
};

// Stores resource bindings and applies them via glBind* calls when bound
class GLDescriptorSet : public RHIDescriptorSet {
public:
    GLDescriptorSet() = default;
    ~GLDescriptorSet() override = default;

    bool init(const GLDescriptorSetLayout* layout);

    void update(const DescriptorWrite* writes, uint32_t count) override;
    const RHIDescriptorSetLayout* layout() const override { return m_layout; }

    // Bind all resources in this descriptor set to their respective binding points
    // Called internally when the descriptor set is bound to a command buffer
    void apply() const;

private:
    const GLDescriptorSetLayout* m_layout = nullptr;
    std::vector<GLBoundResource> m_resources;
};

}

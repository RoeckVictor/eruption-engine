#pragma once

#include "RHITypes.h"
#include <cstdint>
#include <vector>

namespace engine::rhi {

class RHIBuffer;
class RHITexture;

enum class DescriptorType {
    UniformBuffer,
    StorageBuffer,
    Texture,
    StorageImage,
    Sampler,
    CombinedImageSampler
};

struct DescriptorBinding {
    uint32_t binding;
    DescriptorType type;
    ShaderStage stage;
    uint32_t count = 1;
};

// Descriptor set layout - defines the structure of a descriptor set
// Created once, used to create multiple descriptor sets with the same layout
class RHIDescriptorSetLayout {
public:
    virtual ~RHIDescriptorSetLayout() = default;

    RHIDescriptorSetLayout(const RHIDescriptorSetLayout&) = delete;
    RHIDescriptorSetLayout& operator=(const RHIDescriptorSetLayout&) = delete;

    virtual const std::vector<DescriptorBinding>& bindings() const = 0;

protected:
    RHIDescriptorSetLayout() = default;
};

struct DescriptorWrite {
    uint32_t binding;
    uint32_t array_element = 0;
    DescriptorType type;

    RHIBuffer* buffer = nullptr;
    size_t buffer_offset = 0;
    size_t buffer_range = 0;

    RHITexture* texture = nullptr;
    uint32_t mip_level = 0;
    BufferAccess access = BufferAccess::ReadWrite;
};

// Descriptor set - a group of bound resources
class RHIDescriptorSet {
public:
    virtual ~RHIDescriptorSet() = default;

    RHIDescriptorSet(const RHIDescriptorSet&) = delete;
    RHIDescriptorSet& operator=(const RHIDescriptorSet&) = delete;

    virtual void update(const DescriptorWrite* writes, uint32_t count) = 0;
    void update(const DescriptorWrite& write) { update(&write, 1); }

    virtual const RHIDescriptorSetLayout* layout() const = 0;

protected:
    RHIDescriptorSet() = default;
};

struct DescriptorSetLayoutDesc {
    std::vector<DescriptorBinding> bindings;
};

}

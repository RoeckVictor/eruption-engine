#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHIShader.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <filesystem>

namespace engine::rhi {

class VKDevice;

class VKShader : public RHIShader {
public:
    // Minimum guaranteed push constant size in Vulkan (128 bytes)
    static constexpr uint32_t MAX_PUSH_CONSTANT_SIZE = 128;

    struct ReflectedBinding {
        uint32_t binding;
        VkDescriptorType type;
        VkShaderStageFlags stages;
    };

    VKShader() = default;
    ~VKShader() override;

    bool init(VKDevice* device, const ShaderDesc& desc);
    bool init_graphics(VKDevice* device, const char* vert_path, const char* frag_path);
    bool init_compute(VKDevice* device, const char* comp_path);

    void bind() override;

    void set_int(const char* name, int value) override;
    void set_uint(const char* name, uint32_t value) override;
    void set_float(const char* name, float value) override;
    void set_vec2(const char* name, float x, float y) override;
    void set_vec3(const char* name, float x, float y, float z) override;
    void set_vec4(const char* name, float x, float y, float z, float w) override;
    void set_mat3(const char* name, const float* value, bool transpose = false) override;
    void set_mat4(const char* name, const float* value, bool transpose = false) override;

    bool try_reload() override;
    void* native_handle() const override { return nullptr; }

    // Vulkan-specific accessors
    VkShaderModule vert_module() const { return m_vert_module; }
    VkShaderModule frag_module() const { return m_frag_module; }
    VkShaderModule comp_module() const { return m_comp_module; }
    VkPipelineLayout pipeline_layout() const { return m_pipeline_layout; }
    VkDescriptorSetLayout descriptor_set_layout() const { return m_descriptor_set_layout; }

    const std::vector<VkPipelineShaderStageCreateInfo>& stage_infos() const { return m_stage_infos; }
    VkPipeline compute_pipeline() const { return m_compute_pipeline; }

    // Push constant access (used by VKContext before draw calls)
    bool has_push_constants() const { return m_push_constant_size > 0; }
    uint32_t push_constant_size() const { return m_push_constant_size; }
    const uint8_t* push_constant_data() const { return m_push_data; }
    bool push_constants_dirty() const { return m_push_dirty; }
    void clear_push_dirty() { m_push_dirty = false; }
    VkShaderStageFlags push_constant_stages() const { return m_push_constant_stages; }
    bool uses_sampler() const { return m_uses_sampler; }
    const std::vector<ReflectedBinding>& reflected_bindings() const { return m_reflected_bindings; }

    // Incremented each time the shader is hot-reloaded.
    // VKPipeline uses this to detect when it needs to recreate its VkPipeline.
    uint32_t reload_version() const { return m_reload_version; }

private:
    struct PushConstantEntry {
        uint32_t offset;
        uint32_t size;
    };

    bool load_spirv(const char* path, std::vector<uint32_t>& out_code);
    VkShaderModule create_module(const std::vector<uint32_t>& code);
    bool create_pipeline_layout();
    void reflect_push_constants(const std::vector<uint32_t>& spirv, VkShaderStageFlags stage);
    void reflect_descriptors(const std::vector<uint32_t>& spirv, VkShaderStageFlags stage);
    void destroy();

    // Write raw bytes into the push constant staging buffer
    void write_push_constant(const char* name, const void* data, uint32_t size);

    VKDevice* m_device = nullptr;

    VkShaderModule m_vert_module = VK_NULL_HANDLE;
    VkShaderModule m_frag_module = VK_NULL_HANDLE;
    VkShaderModule m_comp_module = VK_NULL_HANDLE;

    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
    VkPipeline m_compute_pipeline = VK_NULL_HANDLE; // Cached compute pipeline (compute shaders only)

    std::vector<VkPipelineShaderStageCreateInfo> m_stage_infos;

    // Source paths and timestamps for hot-reload
    std::string m_vert_path;
    std::string m_frag_path;
    std::string m_comp_path;

    using FileTime = std::filesystem::file_time_type;
    FileTime m_vert_time;
    FileTime m_frag_time;
    FileTime m_comp_time;

    static FileTime safe_last_write(const std::string& path);
    void snapshot_times();
    bool files_changed() const;
    bool compile_glsl_to_spirv(const std::string& glsl_path) const;

    // Push constant reflection data
    std::unordered_map<std::string, PushConstantEntry> m_push_map;
    uint8_t m_push_data[MAX_PUSH_CONSTANT_SIZE] = {};
    uint32_t m_push_constant_size = 0;
    VkShaderStageFlags m_push_constant_stages = 0; // Accumulated from reflected stages
    bool m_uses_sampler = false;
    bool m_push_dirty = false;

    // Descriptor binding reflection data (SSBOs, storage images, samplers)
    std::vector<ReflectedBinding> m_reflected_bindings;

    uint32_t m_reload_version = 0;
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT

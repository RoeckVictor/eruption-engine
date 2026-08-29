#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKShader.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include "VKContext.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/core/Log.h"
#include <fstream>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace engine::rhi {

VKShader::~VKShader() {
    destroy();
}

void VKShader::destroy() {
    if (!m_device) return;
    VkDevice dev = m_device->device();

    VkPipeline compute_pipeline = m_compute_pipeline;
    VkPipelineLayout pipeline_layout = m_pipeline_layout;
    VkDescriptorSetLayout ds_layout = m_descriptor_set_layout;
    VkShaderModule vert = m_vert_module;
    VkShaderModule frag = m_frag_module;
    VkShaderModule comp = m_comp_module;

    if (compute_pipeline || pipeline_layout || ds_layout || vert || frag || comp) {
        m_device->defer_deletion([dev, compute_pipeline, pipeline_layout, ds_layout, vert, frag, comp]() {
            if (compute_pipeline) vkDestroyPipeline(dev, compute_pipeline, nullptr);
            if (pipeline_layout)  vkDestroyPipelineLayout(dev, pipeline_layout, nullptr);
            if (ds_layout)        vkDestroyDescriptorSetLayout(dev, ds_layout, nullptr);
            if (vert)             vkDestroyShaderModule(dev, vert, nullptr);
            if (frag)             vkDestroyShaderModule(dev, frag, nullptr);
            if (comp)             vkDestroyShaderModule(dev, comp, nullptr);
        });
    }

    m_compute_pipeline = VK_NULL_HANDLE;
    m_pipeline_layout = VK_NULL_HANDLE;
    m_descriptor_set_layout = VK_NULL_HANDLE;
    m_vert_module = VK_NULL_HANDLE;
    m_frag_module = VK_NULL_HANDLE;
    m_comp_module = VK_NULL_HANDLE;

    m_stage_infos.clear();
    m_valid = false;
}

bool VKShader::load_spirv(const char* path, std::vector<uint32_t>& out_code) {
    // Try loading .spv file (append .spv if not already present)
    std::string spv_path = path;
    if (spv_path.size() < 4 || spv_path.substr(spv_path.size() - 4) != ".spv") {
        spv_path += ".spv";
    }

    std::ifstream file(spv_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ENGINE_ERR("VKShader: Cannot open SPIR-V file '%s'", spv_path.c_str());
        return false;
    }

    auto file_size = file.tellg();
    if (file_size <= 0 || file_size % 4 != 0) {
        ENGINE_ERR("VKShader: Invalid SPIR-V file size for '%s'", spv_path.c_str());
        return false;
    }

    out_code.resize(static_cast<size_t>(file_size) / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(out_code.data()), file_size);
    return true;
}

VkShaderModule VKShader::create_module(const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size() * sizeof(uint32_t);
    create_info.pCode = code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (!VK_CHECK(vkCreateShaderModule(m_device->device(), &create_info, nullptr, &module))) {
        return VK_NULL_HANDLE;
    }
    return module;
}

bool VKShader::create_pipeline_layout() {
    // Build descriptor set layout from reflected bindings (SSBOs, images, samplers)
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (const auto& rb : m_reflected_bindings) {
        VkDescriptorSetLayoutBinding b = {};
        b.binding = rb.binding;
        b.descriptorType = rb.type;
        b.descriptorCount = 1;
        b.stageFlags = rb.stages;
        bindings.push_back(b);
    }

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.empty() ? nullptr : bindings.data();

    if (!VK_CHECK(vkCreateDescriptorSetLayout(m_device->device(), &layout_info, nullptr, &m_descriptor_set_layout))) {
        return false;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (!bindings.empty()) {
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    }

    // Only declare push constant range if the shader actually uses push constants
    VkPushConstantRange push_range = {};
    if (m_push_constant_size > 0) {
        push_range.stageFlags = m_push_constant_stages;
        push_range.offset = 0;
        // Round up to 4-byte alignment (Vulkan spec requirement)
        push_range.size = (m_push_constant_size + 3u) & ~3u;
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_range;
    }

    if (!VK_CHECK(vkCreatePipelineLayout(m_device->device(), &pipeline_layout_info, nullptr, &m_pipeline_layout))) {
        return false;
    }

    return true;
}

bool VKShader::init(VKDevice* device, const ShaderDesc& desc) {
    m_device = device;

    for (uint32_t i = 0; i < desc.stage_count; ++i) {
        const auto& stage = desc.stages[i];
        std::vector<uint32_t> code;

        // Prefer precompiled SPIR-V
        if (stage.spirv_code && stage.spirv_size > 0) {
            code.assign(stage.spirv_code, stage.spirv_code + stage.spirv_size / sizeof(uint32_t));
        } else if (stage.source_path) {
            if (!load_spirv(stage.source_path, code)) return false;
        } else {
            ENGINE_ERR("VKShader: No SPIR-V source for shader stage");
            return false;
        }

        VkShaderModule module = create_module(code);
        if (module == VK_NULL_HANDLE) return false;

        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.pName = stage.entry_point ? stage.entry_point : "main";
        stage_info.module = module;

        switch (stage.stage) {
            case ShaderStage::Vertex:
                stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
                m_vert_module = module;
                break;
            case ShaderStage::Fragment:
                stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                m_frag_module = module;
                break;
            case ShaderStage::Compute:
                stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                m_comp_module = module;
                m_is_compute = true;
                break;
            default:
                ENGINE_ERR("VKShader: Unsupported shader stage");
                vkDestroyShaderModule(device->device(), module, nullptr);
                return false;
        }

        m_stage_infos.push_back(stage_info);
    }

    if (!create_pipeline_layout()) return false;

    m_valid = true;
    return true;
}

bool VKShader::init_graphics(VKDevice* device, const char* vert_path, const char* frag_path) {
    m_device = device;
    m_vert_path = vert_path ? vert_path : "";
    m_frag_path = frag_path ? frag_path : "";

    std::vector<uint32_t> vert_code, frag_code;
    if (!load_spirv(vert_path, vert_code) || !load_spirv(frag_path, frag_code)) {
        return false;
    }

    m_vert_module = create_module(vert_code);
    m_frag_module = create_module(frag_code);
    if (!m_vert_module || !m_frag_module) return false;

    VkPipelineShaderStageCreateInfo vert_stage = {};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = m_vert_module;
    vert_stage.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage = {};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = m_frag_module;
    frag_stage.pName = "main";

    m_stage_infos = {vert_stage, frag_stage};

    // Reflect push constants and descriptor bindings from both stages
    reflect_push_constants(vert_code, VK_SHADER_STAGE_VERTEX_BIT);
    reflect_push_constants(frag_code, VK_SHADER_STAGE_FRAGMENT_BIT);
    reflect_descriptors(vert_code, VK_SHADER_STAGE_VERTEX_BIT);
    reflect_descriptors(frag_code, VK_SHADER_STAGE_FRAGMENT_BIT);

    if (!create_pipeline_layout()) return false;

    snapshot_times();
    m_is_compute = false;
    m_valid = true;
    return true;
}

bool VKShader::init_compute(VKDevice* device, const char* comp_path) {
    m_device = device;
    m_comp_path = comp_path ? comp_path : "";

    std::vector<uint32_t> code;
    if (!load_spirv(comp_path, code)) return false;

    m_comp_module = create_module(code);
    if (!m_comp_module) return false;

    reflect_push_constants(code, VK_SHADER_STAGE_COMPUTE_BIT);
    reflect_descriptors(code, VK_SHADER_STAGE_COMPUTE_BIT);

    VkPipelineShaderStageCreateInfo comp_stage = {};
    comp_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    comp_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    comp_stage.module = m_comp_module;
    comp_stage.pName = "main";

    m_stage_infos = {comp_stage};

    if (!create_pipeline_layout()) return false;

    // Create and cache compute pipeline (Vulkan requires an explicit VkPipeline for compute)
    VkComputePipelineCreateInfo compute_info = {};
    compute_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_info.stage = comp_stage;
    compute_info.layout = m_pipeline_layout;

    if (!VK_CHECK(vkCreateComputePipelines(device->device(), VK_NULL_HANDLE, 1,
                                           &compute_info, nullptr, &m_compute_pipeline))) {
        return false;
    }

    snapshot_times();
    m_is_compute = true;
    m_valid = true;
    return true;
}

// ---- SPIR-V push constant reflection ----
// Parse the SPIR-V binary to extract push constant member names and byte offsets.
// This avoids hardcoding layouts or needing a full SPIR-V reflection library.

void VKShader::reflect_push_constants(const std::vector<uint32_t>& spirv, VkShaderStageFlags stage) {
    if (spirv.size() < 5) return;

    // SPIR-V opcodes we care about
    constexpr uint32_t SpvOpMemberName = 6;
    constexpr uint32_t SpvOpTypePointer = 32;
    constexpr uint32_t SpvOpVariable = 59;
    constexpr uint32_t SpvOpMemberDecorate = 72;
    constexpr uint32_t SpvDecorationOffset = 35;
    constexpr uint32_t SpvStorageClassPushConstant = 9;

    // Pass 1: Find the push constant variable's type ID
    uint32_t push_constant_pointer_type = 0;
    uint32_t push_constant_struct_type = 0;

    // Find OpVariable with PushConstant storage class
    size_t i = 5; // Skip SPIR-V header (5 words)
    while (i < spirv.size()) {
        uint32_t word = spirv[i];
        uint16_t opcode = word & 0xFFFF;
        uint16_t word_count = word >> 16;
        if (word_count == 0) break;

        if (opcode == SpvOpVariable && word_count >= 4) {
            uint32_t result_type = spirv[i + 1];
            uint32_t storage_class = spirv[i + 3];
            if (storage_class == SpvStorageClassPushConstant) {
                push_constant_pointer_type = result_type;
            }
        }
        i += word_count;
    }

    if (push_constant_pointer_type == 0) return; // No push constants in this shader

    // This stage uses push constants — accumulate for the pipeline layout range
    m_push_constant_stages |= stage;

    // Find OpTypePointer to get the struct type
    i = 5;
    while (i < spirv.size()) {
        uint32_t word = spirv[i];
        uint16_t opcode = word & 0xFFFF;
        uint16_t word_count = word >> 16;
        if (word_count == 0) break;

        if (opcode == SpvOpTypePointer && word_count >= 4) {
            uint32_t result_id = spirv[i + 1];
            if (result_id == push_constant_pointer_type) {
                push_constant_struct_type = spirv[i + 3];
            }
        }
        i += word_count;
    }

    if (push_constant_struct_type == 0) return;

    // Pass 2: Collect member names and offsets for this struct type
    std::unordered_map<uint32_t, std::string> member_names;
    std::unordered_map<uint32_t, uint32_t> member_offsets;

    i = 5;
    while (i < spirv.size()) {
        uint32_t word = spirv[i];
        uint16_t opcode = word & 0xFFFF;
        uint16_t word_count = word >> 16;
        if (word_count == 0) break;

        if (opcode == SpvOpMemberName && word_count >= 4) {
            uint32_t type_id = spirv[i + 1];
            uint32_t member_index = spirv[i + 2];
            if (type_id == push_constant_struct_type) {
                // Extract null-terminated string from remaining words
                const char* str = reinterpret_cast<const char*>(&spirv[i + 3]);
                member_names[member_index] = str;
            }
        }

        if (opcode == SpvOpMemberDecorate && word_count >= 5) {
            uint32_t type_id = spirv[i + 1];
            uint32_t member_index = spirv[i + 2];
            uint32_t decoration = spirv[i + 3];
            if (type_id == push_constant_struct_type && decoration == SpvDecorationOffset) {
                member_offsets[member_index] = spirv[i + 4];
            }
        }

        i += word_count;
    }

    // Build the push constant map
    for (auto& [idx, name] : member_names) {
        auto offset_it = member_offsets.find(idx);
        if (offset_it == member_offsets.end()) continue;

        uint32_t offset = offset_it->second;

        // Compute size from gap to next member or cap at 128
        uint32_t next_offset = MAX_PUSH_CONSTANT_SIZE;
        for (auto& [other_idx, other_off] : member_offsets) {
            if (other_off > offset && other_off < next_offset) {
                next_offset = other_off;
            }
        }
        uint32_t size = next_offset - offset;

        // Don't overwrite if already registered from another stage
        if (m_push_map.find(name) == m_push_map.end()) {
            m_push_map[name] = {offset, size};
        }

        if (offset + size > m_push_constant_size) {
            m_push_constant_size = offset + size;
        }
    }
}

// ---- Uniform setters ----

void VKShader::reflect_descriptors(const std::vector<uint32_t>& spirv, VkShaderStageFlags stage) {
    if (spirv.size() < 5) return;

    constexpr uint32_t SpvOpDecorate = 71;
    constexpr uint32_t SpvOpVariable = 59;
    constexpr uint32_t SpvOpTypePointer = 32;
    constexpr uint32_t SpvOpTypeImage = 25;
    constexpr uint32_t SpvOpTypeSampledImage = 27;
    constexpr uint32_t SpvDecorationBinding = 33;
    constexpr uint32_t SpvStorageClassUniformConstant = 0;
    constexpr uint32_t SpvStorageClassUniform = 2;
    constexpr uint32_t SpvStorageClassStorageBuffer = 12;

    // Pass 1: Collect binding decorations (variable ID → binding number)
    std::unordered_map<uint32_t, uint32_t> id_to_binding;
    size_t i = 5;
    while (i < spirv.size()) {
        uint32_t word = spirv[i];
        uint16_t opcode = word & 0xFFFF;
        uint16_t wc = word >> 16;
        if (wc == 0) break;

        if (opcode == SpvOpDecorate && wc >= 4) {
            uint32_t target_id = spirv[i + 1];
            uint32_t decoration = spirv[i + 2];
            if (decoration == SpvDecorationBinding) {
                id_to_binding[target_id] = spirv[i + 3];
            }
        }
        i += wc;
    }

    // Pass 2: Collect type info — find which type IDs are images vs sampled images
    std::unordered_map<uint32_t, bool> is_image_type;       // type ID → true if image
    std::unordered_map<uint32_t, bool> is_sampled_image;    // type ID → true if sampled image
    std::unordered_map<uint32_t, uint32_t> pointer_to_type; // pointer type ID → pointed-to type ID
    i = 5;
    while (i < spirv.size()) {
        uint32_t word = spirv[i];
        uint16_t opcode = word & 0xFFFF;
        uint16_t wc = word >> 16;
        if (wc == 0) break;

        if (opcode == SpvOpTypeImage && wc >= 2) {
            is_image_type[spirv[i + 1]] = true;
        }
        if (opcode == SpvOpTypeSampledImage && wc >= 2) {
            is_sampled_image[spirv[i + 1]] = true;
        }
        if (opcode == SpvOpTypePointer && wc >= 4) {
            pointer_to_type[spirv[i + 1]] = spirv[i + 3];
        }
        i += wc;
    }

    // Pass 3: Find OpVariable declarations and determine descriptor type
    i = 5;
    while (i < spirv.size()) {
        uint32_t word = spirv[i];
        uint16_t opcode = word & 0xFFFF;
        uint16_t wc = word >> 16;
        if (wc == 0) break;

        if (opcode == SpvOpVariable && wc >= 4) {
            uint32_t result_type = spirv[i + 1];
            uint32_t result_id = spirv[i + 2];
            uint32_t storage_class = spirv[i + 3];

            auto binding_it = id_to_binding.find(result_id);
            if (binding_it != id_to_binding.end()) {
                uint32_t binding = binding_it->second;
                VkDescriptorType desc_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;

                if (storage_class == SpvStorageClassStorageBuffer) {
                    desc_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                } else if (storage_class == SpvStorageClassUniform) {
                    desc_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                } else if (storage_class == SpvStorageClassUniformConstant) {
                    // Check if the pointed-to type is an image or sampled image
                    auto ptr_it = pointer_to_type.find(result_type);
                    if (ptr_it != pointer_to_type.end()) {
                        uint32_t inner_type = ptr_it->second;
                        if (is_sampled_image.count(inner_type)) {
                            desc_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                            m_uses_sampler = true;
                        } else if (is_image_type.count(inner_type)) {
                            desc_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                        }
                    }
                }

                if (desc_type != VK_DESCRIPTOR_TYPE_MAX_ENUM) {
                    // Check if binding already registered (from another stage)
                    bool found = false;
                    for (auto& rb : m_reflected_bindings) {
                        if (rb.binding == binding && rb.type == desc_type) {
                            rb.stages |= stage;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        m_reflected_bindings.push_back({binding, desc_type, stage});
                    }
                }
            }
        }
        i += wc;
    }
}

void VKShader::bind() {
    // For compute shaders, bind the cached compute pipeline through the context
    if (m_is_compute && m_compute_pipeline != VK_NULL_HANDLE) {
        auto* ctx = static_cast<VKContext*>(rhi::get_current_context());
        if (ctx) {
            ctx->bind_compute_shader(this);
        }
    }
    // Graphics shaders are bound as part of the RHIPipeline via bind_pipeline()
}

void VKShader::write_push_constant(const char* name, const void* data, uint32_t size) {
    auto it = m_push_map.find(name);
    if (it == m_push_map.end()) return; // Not a push constant (e.g., sampler uniform) — skip
    const auto& entry = it->second;
    uint32_t copy_size = (size < entry.size) ? size : entry.size;
    memcpy(m_push_data + entry.offset, data, copy_size);
    m_push_dirty = true;
}

void VKShader::set_int(const char* name, int value) {
    write_push_constant(name, &value, sizeof(value));
}

void VKShader::set_uint(const char* name, uint32_t value) {
    write_push_constant(name, &value, sizeof(value));
}

void VKShader::set_float(const char* name, float value) {
    write_push_constant(name, &value, sizeof(value));
}

void VKShader::set_vec2(const char* name, float x, float y) {
    float v[2] = {x, y};
    write_push_constant(name, v, sizeof(v));
}

void VKShader::set_vec3(const char* name, float x, float y, float z) {
    float v[3] = {x, y, z};
    write_push_constant(name, v, sizeof(v));
}

void VKShader::set_vec4(const char* name, float x, float y, float z, float w) {
    float v[4] = {x, y, z, w};
    write_push_constant(name, v, sizeof(v));
}

void VKShader::set_mat3(const char* name, const float* value, bool /*transpose*/) {
    // GLSL std430 packs mat3 as 3 x vec4 (each column padded to 16 bytes).
    // CPU mat3 is 9 consecutive floats — we must pad each column to vec4.
    float padded[12] = {};
    padded[0] = value[0]; padded[1] = value[1]; padded[2] = value[2]; // col 0 + pad
    padded[4] = value[3]; padded[5] = value[4]; padded[6] = value[5]; // col 1 + pad
    padded[8] = value[6]; padded[9] = value[7]; padded[10] = value[8]; // col 2 + pad
    write_push_constant(name, padded, 12 * sizeof(float));
}

void VKShader::set_mat4(const char* name, const float* value, bool /*transpose*/) {
    write_push_constant(name, value, 16 * sizeof(float));
}

VKShader::FileTime VKShader::safe_last_write(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    return ec ? FileTime{} : t;
}

void VKShader::snapshot_times() {
    if (!m_vert_path.empty()) m_vert_time = safe_last_write(m_vert_path);
    if (!m_frag_path.empty()) m_frag_time = safe_last_write(m_frag_path);
    if (!m_comp_path.empty()) m_comp_time = safe_last_write(m_comp_path);
}

bool VKShader::files_changed() const {
    if (!m_vert_path.empty() && safe_last_write(m_vert_path) != m_vert_time) return true;
    if (!m_frag_path.empty() && safe_last_write(m_frag_path) != m_frag_time) return true;
    if (!m_comp_path.empty() && safe_last_write(m_comp_path) != m_comp_time) return true;
    return false;
}

bool VKShader::compile_glsl_to_spirv(const std::string& glsl_path) const {
    std::string spv_path = glsl_path + ".spv";
    std::filesystem::path include_dir = std::filesystem::path(glsl_path).parent_path() / "include";

    // Use platform-specific process creation to avoid shell injection via std::system.
    // Shader paths originate from the engine, but defense in depth is good practice.
#ifdef _WIN32
    std::string cmd = "glslc \"" + glsl_path + "\" -o \"" + spv_path +
        "\" --target-env=vulkan1.2 -I \"" + include_dir.string() + "\"";

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    // CreateProcessA does NOT invoke a shell, so metacharacters like ; | & are not interpreted.
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        ENGINE_ERR("VKShader: Failed to launch glslc for '%s'", glsl_path.c_str());
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code == 0;
#else
    pid_t pid = fork();
    if (pid == 0) {
        // Child: exec glslc directly (no shell)
        execlp("glslc", "glslc",
               glsl_path.c_str(), "-o", spv_path.c_str(),
               "--target-env=vulkan1.2", "-I", include_dir.string().c_str(),
               nullptr);
        _exit(1); // exec failed
    }
    if (pid < 0) {
        ENGINE_ERR("VKShader: fork() failed for glslc");
        return false;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

bool VKShader::try_reload() {
    if (!m_valid || !m_device) return false;
    if (!files_changed()) return false;

    // Recompile all source files to SPIR-V
    if (!m_vert_path.empty() && !compile_glsl_to_spirv(m_vert_path)) return false;
    if (!m_frag_path.empty() && !compile_glsl_to_spirv(m_frag_path)) return false;
    if (!m_comp_path.empty() && !compile_glsl_to_spirv(m_comp_path)) return false;

    // Wait for GPU to finish using old shader
    vkDeviceWaitIdle(m_device->device());

    if (m_is_compute) {
        // Reload compute shader
        std::vector<uint32_t> code;
        if (!load_spirv(m_comp_path.c_str(), code)) return false;

        VkShaderModule new_module = create_module(code);
        if (!new_module) return false;

        // Destroy old resources
        if (m_compute_pipeline) { vkDestroyPipeline(m_device->device(), m_compute_pipeline, nullptr); m_compute_pipeline = VK_NULL_HANDLE; }
        if (m_comp_module) vkDestroyShaderModule(m_device->device(), m_comp_module, nullptr);

        m_comp_module = new_module;

        // Rebuild push constant + descriptor reflection
        m_push_map.clear();
        m_push_constant_size = 0;
        m_push_constant_stages = 0;
        m_reflected_bindings.clear();
        m_uses_sampler = false;
        memset(m_push_data, 0, sizeof(m_push_data));
        reflect_push_constants(code, VK_SHADER_STAGE_COMPUTE_BIT);
        reflect_descriptors(code, VK_SHADER_STAGE_COMPUTE_BIT);

        // Update stage info
        m_stage_infos.clear();
        VkPipelineShaderStageCreateInfo comp_stage = {};
        comp_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        comp_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        comp_stage.module = m_comp_module;
        comp_stage.pName = "main";
        m_stage_infos.push_back(comp_stage);

        // Recreate pipeline layout and compute pipeline
        if (m_pipeline_layout) { vkDestroyPipelineLayout(m_device->device(), m_pipeline_layout, nullptr); m_pipeline_layout = VK_NULL_HANDLE; }
        if (m_descriptor_set_layout) { vkDestroyDescriptorSetLayout(m_device->device(), m_descriptor_set_layout, nullptr); m_descriptor_set_layout = VK_NULL_HANDLE; }
        if (!create_pipeline_layout()) return false;

        VkComputePipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = comp_stage;
        pipeline_info.layout = m_pipeline_layout;
        if (!VK_CHECK(vkCreateComputePipelines(m_device->device(), VK_NULL_HANDLE, 1,
                                               &pipeline_info, nullptr, &m_compute_pipeline))) {
            return false;
        }
    } else {
        // Reload graphics shader
        std::vector<uint32_t> vert_code, frag_code;
        if (!load_spirv(m_vert_path.c_str(), vert_code) || !load_spirv(m_frag_path.c_str(), frag_code))
            return false;

        VkShaderModule new_vert = create_module(vert_code);
        VkShaderModule new_frag = create_module(frag_code);
        if (!new_vert || !new_frag) {
            if (new_vert) vkDestroyShaderModule(m_device->device(), new_vert, nullptr);
            if (new_frag) vkDestroyShaderModule(m_device->device(), new_frag, nullptr);
            return false;
        }

        if (m_vert_module) vkDestroyShaderModule(m_device->device(), m_vert_module, nullptr);
        if (m_frag_module) vkDestroyShaderModule(m_device->device(), m_frag_module, nullptr);
        m_vert_module = new_vert;
        m_frag_module = new_frag;

        // Rebuild reflection
        m_push_map.clear();
        m_push_constant_size = 0;
        m_push_constant_stages = 0;
        m_reflected_bindings.clear();
        m_uses_sampler = false;
        memset(m_push_data, 0, sizeof(m_push_data));
        reflect_push_constants(vert_code, VK_SHADER_STAGE_VERTEX_BIT);
        reflect_push_constants(frag_code, VK_SHADER_STAGE_FRAGMENT_BIT);
        reflect_descriptors(vert_code, VK_SHADER_STAGE_VERTEX_BIT);
        reflect_descriptors(frag_code, VK_SHADER_STAGE_FRAGMENT_BIT);

        // Update stage infos
        m_stage_infos.clear();
        VkPipelineShaderStageCreateInfo vs = {};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vs.module = m_vert_module;
        vs.pName = "main";
        VkPipelineShaderStageCreateInfo fs = {};
        fs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fs.module = m_frag_module;
        fs.pName = "main";
        m_stage_infos = {vs, fs};

        // Recreate pipeline layout
        if (m_pipeline_layout) { vkDestroyPipelineLayout(m_device->device(), m_pipeline_layout, nullptr); m_pipeline_layout = VK_NULL_HANDLE; }
        if (m_descriptor_set_layout) { vkDestroyDescriptorSetLayout(m_device->device(), m_descriptor_set_layout, nullptr); m_descriptor_set_layout = VK_NULL_HANDLE; }
        if (!create_pipeline_layout()) return false;

        // Note: graphics VkPipeline is owned by VKPipeline, not VKShader
        // Pipeline recreation would need to happen at the VKPipeline level
    }

    snapshot_times();
    ++m_reload_version;
    ENGINE_LOG("VKShader: Hot-reloaded successfully");
    return true;
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT

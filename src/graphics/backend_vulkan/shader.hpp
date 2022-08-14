#pragma once

#include <volk.h>

#include "core/container.hpp"
#include "core/ptr.hpp"
#include "graphics/defines.hpp"
#include "graphics/shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct ShaderBindingsVulkan {
    struct ViewInfo {
        VkImageViewType image_view_type;
    };

    Vec<Vec<VkDescriptorSetLayoutBinding>> bindings;
    Vec<Vec<ViewInfo>> bindings_view_info;
    HashMap<std::string, std::pair<uint8_t, uint16_t>> name_map;

    uint32_t push_constant_size;

    void Combine(const ShaderBindingsVulkan &another);
};

struct ShaderInfoVulkan {
    Vec<ResourceFormat> vertex_inputs;

    uint32_t compute_local_size_x;
    uint32_t compute_local_size_y;
    uint32_t compute_local_size_z;

    ShaderBindingsVulkan bindings;
};

class ShaderModuleVulkan : public ShaderModule {
public:
    ShaderModuleVulkan(Ref<class DeviceVulkan> device, const Vec<uint8_t> &src_bytes);
    ~ShaderModuleVulkan() override;

    VkShaderModule Raw() const { return shader_module_; }

    const VkPipelineShaderStageCreateInfo &RawPipelineShaderStage() const { return pipeline_shader_stage_; }

    const ShaderInfoVulkan &Info() const { return info_; }

private:
    Ref<DeviceVulkan> device_;
    VkShaderModule shader_module_;
    std::string entry_point_;
    VkPipelineShaderStageCreateInfo pipeline_shader_stage_;
    ShaderInfoVulkan info_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

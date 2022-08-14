#include "shader.hpp"

#include <spirv_reflect.h>

#include "device.hpp"
#include "utils.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

VkShaderStageFlagBits FromSpvShaderStageBits(SpvReflectShaderStageFlagBits stage) {
    return static_cast<VkShaderStageFlagBits>(stage);
}
VkShaderStageFlags FromSpvShaderStage(SpvReflectShaderStageFlagBits stage) {
    return static_cast<VkShaderStageFlags>(stage);
}

ResourceFormat FromSpvReflectFormat(SpvReflectFormat format) {
    switch (format) {
        case SPV_REFLECT_FORMAT_UNDEFINED: return ResourceFormat::eUndefined;
        case SPV_REFLECT_FORMAT_R32_UINT: return ResourceFormat::eR32UInt;
        case SPV_REFLECT_FORMAT_R32_SINT: return ResourceFormat::eR32SInt;
        case SPV_REFLECT_FORMAT_R32_SFLOAT: return ResourceFormat::eR32SFloat;
        case SPV_REFLECT_FORMAT_R32G32_UINT: return ResourceFormat::eRg32UInt;
        case SPV_REFLECT_FORMAT_R32G32_SINT: return ResourceFormat::eRg32SInt;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT: return ResourceFormat::eRg32SFloat;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT: return ResourceFormat::eRgb32UInt;
        case SPV_REFLECT_FORMAT_R32G32B32_SINT: return ResourceFormat::eRgb32SInt;
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT: return ResourceFormat::eRgb32SFloat;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT: return ResourceFormat::eRgba32UInt;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT: return ResourceFormat::eRgba32SInt;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: return ResourceFormat::eRgba32SFloat;
        default: return ResourceFormat::eUndefined;
    }
}

VkDescriptorType FromSpvDescriptorType(SpvReflectDescriptorType type) {
    switch (type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

VkImageViewType FromSpvDim(SpvDim dim, bool is_array) {
    switch (dim) {
        case SpvDim1D: return is_array ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        case SpvDim2D: return is_array ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        case SpvDim3D: return VK_IMAGE_VIEW_TYPE_3D;
        case SpvDimCube: return is_array ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        default: Unreachable();
    }
}

}

void ShaderBindingsVulkan::Combine(const ShaderBindingsVulkan &another) {
    if (another.bindings.size() > bindings.size()) {
        bindings.resize(another.bindings.size());
    }

    for (size_t set = 0; set < another.bindings.size(); set++) {
        if (another.bindings[set].size() > bindings[set].size()) {
            bindings[set].resize(another.bindings[set].size());
        }

        for (size_t binding = 0; binding < another.bindings[set].size(); binding++) {
            auto &this_binding = bindings[set][binding];
            const auto &another_binding = another.bindings[set][binding];
            if (another_binding.descriptorType != VK_DESCRIPTOR_TYPE_MAX_ENUM) {
                if (this_binding.descriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM) {
                    this_binding = another_binding;
                } else {
                    this_binding.stageFlags |= another_binding.stageFlags;
                    this_binding.descriptorCount =
                        std::max(this_binding.descriptorCount, another_binding.descriptorCount);
                }
            }
        }
    }

    name_map.reserve(name_map.size() + another.name_map.size());
    for (const auto &[name, set_binding] : another.name_map) {
        name_map[name] = set_binding;
    }

    push_constant_size = std::max(push_constant_size, another.push_constant_size);
}

ShaderModuleVulkan::ShaderModuleVulkan(Ref<DeviceVulkan> device, const Vec<uint8_t> &src_bytes)
    : device_(device) {
    VkShaderModuleCreateInfo shader_module_ci {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = src_bytes.size(),
        .pCode = reinterpret_cast<const uint32_t *>(src_bytes.data()),
    };
    vkCreateShaderModule(device_->Raw(), &shader_module_ci, nullptr, &shader_module_);

    spv_reflect::ShaderModule reflect_module(src_bytes);

    entry_point_ = reflect_module.GetEntryPointName();
    pipeline_shader_stage_ = VkPipelineShaderStageCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = FromSpvShaderStageBits(reflect_module.GetShaderStage()),
        .module = shader_module_,
        .pName = entry_point_.c_str(),
        .pSpecializationInfo = nullptr,
    };

    if (reflect_module.GetShaderStage() == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
        uint32_t num_inputs = 0;
        reflect_module.EnumerateInputVariables(&num_inputs, nullptr);
        Vec<SpvReflectInterfaceVariable *> inputs(num_inputs);
        reflect_module.EnumerateInputVariables(&num_inputs, inputs.data());

        uint32_t max_location = 0;
        for (auto input : inputs) {
            max_location = std::max(max_location, input->location);
        }
        info_.vertex_inputs.resize(max_location, ResourceFormat::eUndefined);
        for (auto input : inputs) {
            info_.vertex_inputs[input->location] = FromSpvReflectFormat(input->format);
        }
    }

    if (reflect_module.GetShaderStage() == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) {
        const auto entry_point_info = spvReflectGetEntryPoint(&reflect_module.GetShaderModule(), entry_point_.c_str());
        info_.compute_local_size_x = entry_point_info->local_size.x;
        info_.compute_local_size_y = entry_point_info->local_size.y;
        info_.compute_local_size_z = entry_point_info->local_size.z;
    }

    uint32_t num_bindings = 0;
    reflect_module.EnumerateDescriptorBindings(&num_bindings, nullptr);
    Vec<SpvReflectDescriptorBinding *> bindings(num_bindings);
    reflect_module.EnumerateDescriptorBindings(&num_bindings, bindings.data());

    info_.bindings.name_map.reserve(num_bindings);
    uint32_t max_set = 0;
    for (auto binding : bindings) {
        max_set = std::max(max_set, binding->set);
    }
    info_.bindings.bindings.resize(max_set);
    Vec<uint32_t> max_binding(max_set, 0);
    for (auto binding : bindings) {
        max_binding[binding->set] = std::max(max_binding[binding->set], binding->binding);
    }
    for (uint32_t set = 0; set < max_set; set++) {
        info_.bindings.bindings[set].resize(max_binding[set],
            VkDescriptorSetLayoutBinding { .descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM });
    }
    for (auto binding : bindings) {
        info_.bindings.bindings[binding->set][binding->binding] = VkDescriptorSetLayoutBinding {
            .binding = binding->binding,
            .descriptorType = FromSpvDescriptorType(binding->descriptor_type),
            .descriptorCount = binding->array.dims_count > 0 ? binding->array.dims[0] : 1,
            .stageFlags = FromSpvShaderStage(reflect_module.GetShaderStage()),
            .pImmutableSamplers = nullptr,
        };
        info_.bindings.name_map[binding->name] = { binding->set, binding->binding };

        if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE
            || binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
            info_.bindings.bindings_view_info[binding->set][binding->binding] = ShaderBindingsVulkan::ViewInfo {
                .image_view_type = FromSpvDim(binding->image.dim, binding->image.arrayed),
            };
        }
    }

    uint32_t num_push_constants;
    reflect_module.EnumeratePushConstantBlocks(&num_push_constants, nullptr);
    Vec<SpvReflectBlockVariable *> push_constants(num_push_constants);
    reflect_module.EnumeratePushConstantBlocks(&num_push_constants, push_constants.data());
    info_.bindings.push_constant_size = 0;
    for (auto push_c : push_constants) {
        info_.bindings.push_constant_size =
            std::max(info_.bindings.push_constant_size, push_c->offset + push_c->size);
    }
}

ShaderModuleVulkan::~ShaderModuleVulkan() {
    vkDestroyShaderModule(device_->Raw(), shader_module_, nullptr);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

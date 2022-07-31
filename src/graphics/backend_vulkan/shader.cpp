#include "shader.hpp"

#include <spirv_reflect.h>

#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

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

    // TODO - reflection
}

ShaderModuleVulkan::~ShaderModuleVulkan() {
    vkDestroyShaderModule(device_->Raw(), shader_module_, nullptr);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

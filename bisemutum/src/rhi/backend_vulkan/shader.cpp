#include "shader.hpp"

#include "volk.h"
#include "device.hpp"
#include "utils.hpp"

namespace bi::rhi {

ShaderModuleVulkan::ShaderModuleVulkan(Ref<DeviceVulkan> device, ShaderModuleDesc const& desc) : device_(device) {
    VkShaderModuleCreateInfo shader_module_ci {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = desc.binary_data.size(),
        .pCode = reinterpret_cast<uint32_t const*>(desc.binary_data.data()),
    };
    vkCreateShaderModule(device_->raw(), &shader_module_ci, nullptr, &shader_module_);
}

ShaderModuleVulkan::~ShaderModuleVulkan() {
    if (shader_module_) {
        vkDestroyShaderModule(device_->raw(), shader_module_, nullptr);
        shader_module_ = VK_NULL_HANDLE;
    }
}

}

#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/prelude/ref.hpp>
#include <bisemutum/rhi/shader.hpp>

namespace bi::rhi {

struct ShaderModuleVulkan final : ShaderModule {
    ShaderModuleVulkan(Ref<struct DeviceVulkan> device, ShaderModuleDesc const& desc);
    ~ShaderModuleVulkan() override;

    auto raw() const -> VkShaderModule { return shader_module_; }

private:
    Ref<DeviceVulkan> device_;
    VkShaderModule shader_module_ = VK_NULL_HANDLE;
};

}

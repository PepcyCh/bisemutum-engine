#pragma once

#include <volk.h>

#include "core/container.hpp"
#include "graphics/shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class ShaderModuleVulkan : public ShaderModule {
public:
    ShaderModuleVulkan(Ref<class DeviceVulkan> device, const Vec<uint8_t> &src_bytes);
    ~ShaderModuleVulkan() override;

    VkShaderModule Raw() const { return shader_module_; }

private:
    Ref<DeviceVulkan> device_;
    VkShaderModule shader_module_;

    // TODO - reflection infos
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

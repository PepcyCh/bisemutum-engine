#pragma once

#include <volk.h>

#include "core/ptr.hpp"
#include "graphics/sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class SamplerVulkan : public Sampler {
public:
    SamplerVulkan(Ref<class DeviceVulkan> device, const SamplerDesc &desc);
    ~SamplerVulkan() override;

private:
    Ref<DeviceVulkan> device_;
    VkSampler sampler_ = VK_NULL_HANDLE;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

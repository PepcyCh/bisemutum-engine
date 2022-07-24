#pragma once

#include <volk.h>

#include "graphics/sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class SamplerVulkan : public Sampler {
public:
    SamplerVulkan(class DeviceVulkan *device, const SamplerDesc &desc);
    ~SamplerVulkan() override;

private:
    DeviceVulkan *device_;
    VkSampler sampler_ = VK_NULL_HANDLE;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

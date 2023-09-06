#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/prelude/ref.hpp>
#include <bisemutum/rhi/sampler.hpp>

namespace bi::rhi {

struct SamplerVulkan final : Sampler {
    SamplerVulkan(Ref<struct DeviceVulkan> device, SamplerDesc const& desc);
    ~SamplerVulkan() override;

    auto raw() const -> VkSampler { return sampler_; }

private:
    Ref<DeviceVulkan> device_;
    VkSampler sampler_ = VK_NULL_HANDLE;
};

}

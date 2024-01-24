#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/rhi/accel.hpp>

namespace bi::rhi {

struct AccelerationStructureVulkan final : AccelerationStructure {
    AccelerationStructureVulkan(Ref<struct DeviceVulkan> device, AccelerationStructureDesc const& desc);
    ~AccelerationStructureVulkan() override;

    auto gpu_reference() const -> uint64_t override;

    auto raw() const -> VkAccelerationStructureKHR { return accel_; }

private:
    Ref<DeviceVulkan> device_;
    VkAccelerationStructureKHR accel_ = VK_NULL_HANDLE;
};

}

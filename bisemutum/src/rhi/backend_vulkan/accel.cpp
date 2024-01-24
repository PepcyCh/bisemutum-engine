#include "accel.hpp"

#include "volk.h"
#include "device.hpp"
#include "resource.hpp"

namespace bi::rhi {

AccelerationStructureVulkan::AccelerationStructureVulkan(
    Ref<DeviceVulkan> device, AccelerationStructureDesc const& desc
)
    : device_(device)
{
    desc_ = desc;
    VkAccelerationStructureCreateInfoKHR accel_ci{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = desc_.buffer.cast_to<const BufferVulkan>()->raw(),
        .offset = desc_.buffer_offset,
        .size = desc_.buffer_range_size,
        .type = desc_.type == AccelerationStructureType::bottom_level
            ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
            : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0,
    };
    vkCreateAccelerationStructureKHR(device_->raw(), &accel_ci, nullptr, &accel_);
}

AccelerationStructureVulkan::~AccelerationStructureVulkan() {
    if (accel_) {
        vkDestroyAccelerationStructureKHR(device_->raw(), accel_, nullptr);
        accel_ = VK_NULL_HANDLE;
    }
}

auto AccelerationStructureVulkan::gpu_reference() const -> uint64_t {
    VkAccelerationStructureDeviceAddressInfoKHR accel_bda_info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructure = accel_,
    };
    return vkGetAccelerationStructureDeviceAddressKHR(device_->raw(), &accel_bda_info);
}

}

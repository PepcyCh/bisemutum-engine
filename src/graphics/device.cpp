#include "device.hpp"

#include "backend_vulkan/device.hpp"

#ifdef WIN32
#include "backend_d3d12/device.hpp"
#endif

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

Ptr<Device> Device::CreateDevice(const DeviceDesc &desc) {
    switch (desc.backend) {
        case DeviceBackend::eVulkan: return DeviceVulkan::Create(desc);
#ifdef WIN32
        case DeviceBackend::eD3D12: return DeviceD3D12::Create(desc);
#else
        case DeviceBackend::eD3D12: return nullptr;
#endif
    }
    Unreachable();
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

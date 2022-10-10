#include "graphics/device.hpp"

#include "backend_vulkan/device.hpp"
#ifdef WIN32
#include "backend_d3d12/device.hpp"
#endif

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

Ptr<Device> Device::Create(const DeviceDesc &desc) {
    switch (desc.backend) {
        case GraphicsBackend::eVulkan: return DeviceVulkan::Create(desc);
#ifdef WIN32
        case GraphicsBackend::eD3D12: return DeviceD3D12::Create(desc);
#else
        case GraphicsBackend::eD3D12: BI_CRTICAL(gGraphicsLogger, "D3D12 backend is only supported on Windows");
#endif
    }
    Unreachable();
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

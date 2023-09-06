#include <bisemutum/rhi/device.hpp>
#include <bisemutum/prelude/misc.hpp>

#include "backend_vulkan/device.hpp"
#ifdef _WIN32
#include "backend_d3d12/device.hpp"
#endif

namespace bi::rhi {

auto Device::create(DeviceDesc const& desc) -> Box<Device> {
    switch (desc.backend) {
        case Backend::vulkan: return DeviceVulkan::create(desc);
#ifdef _WIN32
        case Backend::d3d12: return DeviceD3D12::create(desc);
#endif
        default: unreachable();
    }
}

}

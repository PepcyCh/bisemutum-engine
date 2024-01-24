#include "accel.hpp"

#include "resource.hpp"

namespace bi::rhi {

AccelerationStructureD3D12::AccelerationStructureD3D12(
    Ref<DeviceD3D12> device, AccelerationStructureDesc const& desc
)
    : device_(device)
{
    desc_ = desc;
    auto buffer_dx = desc_.buffer.cast_to<const BufferD3D12>();
    gpu_address_ = buffer_dx->raw()->GetGPUVirtualAddress() + desc_.buffer_offset;
}

auto AccelerationStructureD3D12::gpu_reference() const -> uint64_t {
    return gpu_address_;
}

}

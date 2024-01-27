#pragma once

#include <bisemutum/rhi/accel.hpp>
#include <d3d12.h>

namespace bi::rhi {

struct AccelerationStructureD3D12 final : AccelerationStructure {
    AccelerationStructureD3D12(Ref<struct DeviceD3D12> device, AccelerationStructureDesc const& desc);

    auto gpu_reference() const -> uint64_t override;

    auto raw_base_buffer() const -> ID3D12Resource* { return base_buffer_; }

private:
    Ref<DeviceD3D12> device_;
    ID3D12Resource* base_buffer_ = nullptr;
    uint64_t gpu_address_ = 0;
};

}

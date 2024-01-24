#pragma once

#include <bisemutum/rhi/accel.hpp>

namespace bi::rhi {

struct AccelerationStructureD3D12 final : AccelerationStructure {
    AccelerationStructureD3D12(Ref<struct DeviceD3D12> device, AccelerationStructureDesc const& desc);

    auto gpu_reference() const -> uint64_t override;

private:
    Ref<DeviceD3D12> device_;
    uint64_t gpu_address_ = 0;
};

}

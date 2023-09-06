#pragma once

#include <bisemutum/rhi/sync.hpp>
#include <d3d12.h>
#include <wrl.h>

namespace bi::rhi {

struct FenceD3D12 final : Fence {
    FenceD3D12(Ref<struct DeviceD3D12> device);

    auto reset() -> void override;

    auto signal_on(CRef<Queue> queue) -> void override;

    auto wait(uint64_t timeout = ~0ull) -> void override;

    auto is_finished() -> bool override;

    auto raw() const -> ID3D12Fence* { return fence_.Get(); }

private:
    Ref<DeviceD3D12> device_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    uint64_t fence_value_;
};

struct SemaphoreD3D12 final : Semaphore {};

}

#pragma once

#include <bisemutum/rhi/queue.hpp>
#include <d3d12.h>
#include <wrl.h>

namespace bi::rhi {

struct QueueD3D12 final : Queue {
    QueueD3D12(Ref<struct DeviceD3D12> device, D3D12_COMMAND_LIST_TYPE type);

    auto wait_idle() const -> void override;

    auto submit_command_buffer(
        CSpan<Box<CommandBuffer>> cmd_buffers,
        CSpan<CRef<Semaphore>> wait_semaphores,
        CSpan<CRef<Semaphore>> signal_semaphores,
        Option<Ref<Fence>> signal_fence
    ) const -> void override;

    auto raw() const -> ID3D12CommandQueue* { return queue_.Get(); }

private:
    Ref<DeviceD3D12> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_ = nullptr;

    mutable uint64_t fence_value_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_ = nullptr;
};

}

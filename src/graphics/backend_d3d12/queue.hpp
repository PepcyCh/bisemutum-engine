#pragma once

#include "utils.hpp"
#include "graphics/queue.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class QueueD3D12 : public Queue {
public:
    QueueD3D12(class DeviceD3D12 *device, D3D12_COMMAND_LIST_TYPE type);
    ~QueueD3D12() override;

    void WaitIdle() const override;

    void SubmitCommandBuffer(Span<Ptr<CommandBuffer>> &&cmd_buffers, Span<Ref<Semaphore>> wait_semaphores = {},
        Span<Ref<Semaphore>> signal_semaphores = {}, Fence *signal_fence = nullptr) const override;

    ID3D12CommandQueue *Raw() const { return queue_.Get(); }

private:
    DeviceD3D12 *device_;
    ComPtr<ID3D12CommandQueue> queue_;

    mutable UINT64 fence_value_ = 0;
    ComPtr<ID3D12Fence> fence_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

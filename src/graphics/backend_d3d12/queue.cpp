#include "queue.hpp"

#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

QueueD3D12::QueueD3D12(DeviceD3D12 *device, D3D12_COMMAND_LIST_TYPE type) {
    device_ = device;

    D3D12_COMMAND_QUEUE_DESC queue_desc {
        .Type = type,
        .Priority = 1,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };
    device_->Raw()->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_));

    fence_value_ = 0;
    device_->Raw()->CreateFence(fence_value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
}

QueueD3D12::~QueueD3D12() {}

void QueueD3D12::WaitIdle() const {
    ++fence_value_;
    queue_->Signal(fence_.Get(), fence_value_);
    if (fence_->GetCompletedValue() < fence_value_) {
        HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        fence_->SetEventOnCompletion(fence_value_, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }
}

void QueueD3D12::SubmitCommandBuffer(Span<Ptr<CommandBuffer>> &&cmd_buffers, Span<Semaphore *> wait_semaphores,
    Span<Semaphore *> signal_semaphores, Fence *signal_fence) const {
    // TODO
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

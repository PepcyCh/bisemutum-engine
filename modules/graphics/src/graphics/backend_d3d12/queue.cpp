#include "queue.hpp"

#include "device.hpp"
#include "command.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

QueueD3D12::QueueD3D12(Ref<DeviceD3D12> device, D3D12_COMMAND_LIST_TYPE type) : device_(device) {
    D3D12_COMMAND_QUEUE_DESC queue_desc {
        .Type = type,
        .Priority = 0,
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

void QueueD3D12::SubmitCommandBuffer(Span<Ptr<CommandBuffer>> &&cmd_buffers, Span<Ref<Semaphore>> wait_semaphores,
    Span<Ref<Semaphore>> signal_semaphores, Fence *signal_fence) const {
    Vec<ID3D12CommandList *> cmd_lists(cmd_buffers.Size());
    for (size_t i = 0; i < cmd_buffers.Size(); i++) {
        cmd_lists[i] = cmd_buffers[i].AsRef().CastTo<CommandBufferD3D12>()->Raw();
    }
    queue_->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    if (signal_fence) {
        signal_fence->SignalOn(this);
    }
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

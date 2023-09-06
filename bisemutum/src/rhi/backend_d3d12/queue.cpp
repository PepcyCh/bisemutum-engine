#include "queue.hpp"

#include "device.hpp"
#include "command.hpp"

namespace bi::rhi {

QueueD3D12::QueueD3D12(Ref<DeviceD3D12> device, D3D12_COMMAND_LIST_TYPE type) : device_(device) {
    D3D12_COMMAND_QUEUE_DESC queue_desc{
        .Type = type,
        .Priority = 0,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };
    device_->raw()->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_));

    fence_value_ = 0;
    device_->raw()->CreateFence(fence_value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
}

auto QueueD3D12::wait_idle() const -> void {
    ++fence_value_;
    queue_->Signal(fence_.Get(), fence_value_);
    if (fence_->GetCompletedValue() < fence_value_) {
        auto event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        fence_->SetEventOnCompletion(fence_value_, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }
}

auto QueueD3D12::submit_command_buffer(
    CSpan<Box<CommandBuffer>> cmd_buffers,
    CSpan<CRef<Semaphore>> wait_semaphores,
    CSpan<CRef<Semaphore>> signal_semaphores,
    Option<Ref<Fence>> signal_fence
) const -> void {
    std::vector<ID3D12CommandList*> cmd_lists(cmd_buffers.size());
    for (size_t i = 0; i < cmd_buffers.size(); i++) {
        cmd_lists[i] = cmd_buffers[i].ref().cast_to<CommandBufferD3D12>()->raw();
    }
    queue_->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    if (signal_fence) {
        signal_fence.value()->signal_on(unsafe_make_ref(this));
    }
}

}

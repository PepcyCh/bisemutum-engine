#include "sync.hpp"

#include "device.hpp"
#include "queue.hpp"

namespace bi::rhi {

FenceD3D12::FenceD3D12(Ref<DeviceD3D12> device) : device_(device) {
    fence_value_ = 0;
    device_->raw()->CreateFence(fence_value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
}

auto FenceD3D12::reset() -> void {}

auto FenceD3D12::signal_on(CRef<Queue> queue) -> void {
    auto queue_dx = queue.cast_to<const QueueD3D12>();
    ++fence_value_;
    queue_dx->raw()->Signal(fence_.Get(), fence_value_);
}

auto FenceD3D12::wait(uint64_t timeout) -> void {
    if (fence_->GetCompletedValue() < fence_value_) {
        auto event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        fence_->SetEventOnCompletion(fence_value_, event);
        WaitForSingleObject(event, timeout);
        CloseHandle(event);
    }
}

auto FenceD3D12::is_finished() -> bool {
    return fence_->GetCompletedValue() >= fence_value_;
}

}

#include "sync.hpp"

#include "device.hpp"
#include "queue.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

FenceD3D12::FenceD3D12(Ref<DeviceD3D12> device) : device_(device) {
    fence_value_ = 0;
    device_->Raw()->CreateFence(fence_value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
}

FenceD3D12::~FenceD3D12() {}

void FenceD3D12::Reset() {}

void FenceD3D12::SignalOn(const Queue *queue) {
    auto queue_dx = static_cast<const QueueD3D12 *>(queue);
    ++fence_value_;
    queue_dx->Raw()->Signal(fence_.Get(), fence_value_);
}

void FenceD3D12::Wait(uint64_t timeout) {
    if (fence_->GetCompletedValue() < fence_value_) {
        HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        fence_->SetEventOnCompletion(fence_value_, event);
        WaitForSingleObject(event, timeout);
        CloseHandle(event);
    }
}

bool FenceD3D12::IsFinished() {
    return fence_->GetCompletedValue() >= fence_value_;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

#pragma once

#include "utils.hpp"
#include "graphics/sync.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class FenceD3D12 : public Fence {
public:
    FenceD3D12(class DeviceD3D12 *device);
    ~FenceD3D12() override;

    void Reset() override;

    void SignalOn(const Queue *queue) override;

    void Wait(uint64_t timeout = ~0ull) override;

    bool IsFinished() override;

    ID3D12Fence *Raw() const { return fence_.Get(); }

private:
    DeviceD3D12 *device_;
    ComPtr<ID3D12Fence> fence_;
    UINT64 fence_value_;
};

class SemaphoreD3D12 : public Semaphore {};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

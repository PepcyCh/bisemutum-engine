#pragma once

#include "command.hpp"
#include "sync.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

enum class QueueType : uint8_t {
    eGraphics,
    eCompute,
    eTransfer,
};

class Queue {
public:
    virtual ~Queue() = default;

    virtual void WaitIdle() const = 0;

    virtual void SubmitCommandBuffer(Span<Ptr<CommandBuffer>> &&cmd_buffers, Span<Ref<Semaphore>> wait_semaphores = {},
        Span<Ref<Semaphore>> signal_semaphores = {}, Fence *signal_fence = nullptr) const = 0;

protected:
    Queue() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_GFX_NAMESPACE_END

#pragma once

#include "command.hpp"
#include "sync.hpp"

namespace bi::rhi {

enum class QueueType : uint8_t {
    graphics,
    compute,
    transfer,
};

struct Queue {
    virtual ~Queue() = default;

    virtual auto wait_idle() const -> void = 0;

    virtual auto submit_command_buffer(
        CSpan<Box<CommandBuffer>> cmd_buffers,
        CSpan<CRef<Semaphore>> wait_semaphores = {},
        CSpan<CRef<Semaphore>> signal_semaphores = {},
        Option<Ref<Fence>> signal_fence = {}
    ) const -> void = 0;
};

}

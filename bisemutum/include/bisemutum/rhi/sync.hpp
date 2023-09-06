#pragma once

#include "../prelude/ref.hpp"

namespace bi::rhi {

struct Queue;

struct Fence {
    virtual ~Fence() = default;

    virtual auto reset() -> void = 0;

    virtual auto signal_on(CRef<Queue> queue) -> void = 0;

    virtual auto wait(uint64_t timeout = ~0ull) -> void = 0;

    virtual auto is_finished() -> bool = 0;
};

struct Semaphore {
    virtual ~Semaphore() = default;
};

}

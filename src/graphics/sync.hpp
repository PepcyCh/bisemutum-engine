#pragma once

#include <cstdint>

#include "mod.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class Fence {
public:
    virtual ~Fence() = default;

    virtual void Reset() = 0;

    virtual void SignalOn(const class Queue *queue) = 0;

    virtual void Wait(uint64_t timeout = ~0ull) = 0;

    virtual bool IsFinished() = 0;

protected:
    Fence() = default;
};

class Semaphore {
public:
    virtual ~Semaphore() = default;

protected:
    Semaphore() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

#pragma once

#include "core/ptr.hpp"
#include "sync.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class SwapChain {
public:
    virtual ~SwapChain() = default;

    virtual bool AcquireNextTexture(Ref<Semaphore> acquired_semaphore) = 0;

    virtual class Ref<class Texture> GetCurrentTexture() = 0;

protected:
    SwapChain() = default;

    static constexpr uint32_t kNumSwapChainBuffers = 3;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

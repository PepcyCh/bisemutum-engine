#pragma once

#include "sync.hpp"
#include "resource.hpp"
#include "../prelude/span.hpp"
#include "../platform/window.hpp"

namespace bi::rhi {

struct SwapchainDesc final {
    Ref<Queue> queue;
    uint32_t width;
    uint32_t height;
    PlatformWindowHandle window_handle;
};

struct Swapchain {
    virtual ~Swapchain() = default;

    virtual auto resize(uint32_t width, uint32_t height) -> void = 0;

    virtual auto acquire_next_texture(Ref<Semaphore> acquired_semaphore) -> bool = 0;

    virtual auto current_texture() -> Ref<struct Texture> = 0;

    virtual auto present(CSpan<Ref<Semaphore>> wait_semaphores = {}) -> void = 0;
};

}

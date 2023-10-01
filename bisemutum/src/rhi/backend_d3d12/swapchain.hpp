#pragma once

#include <bisemutum/rhi/swapchain.hpp>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "resource.hpp"
#include "queue.hpp"

namespace bi::rhi {

struct SwapchainD3D12 final : Swapchain {
    SwapchainD3D12(Ref<struct DeviceD3D12> device, SwapchainDesc const& desc);

    auto resize(uint32_t width, uint32_t height) -> void override;

    auto acquire_next_texture(Ref<Semaphore> acquired_semaphore) -> bool override;

    auto current_texture() -> Ref<Texture> override;

    auto present(CSpan<Ref<Semaphore>> wait_semaphores = {}) -> void override;

    auto format() const -> ResourceFormat override;

private:
    void create_swapchain_textures();

    Ref<DeviceD3D12> device_;
    Ref<QueueD3D12> queue_;

    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain_;

    uint32_t width_;
    uint32_t height_;
    DXGI_FORMAT surface_format_;

    uint32_t curr_texture_ = 0;
    std::vector<Box<TextureD3D12>> textures_;
};

}

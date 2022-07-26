#pragma once

#include "utils.hpp"
#include "graphics/swap_chain.hpp"
#include "resource.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class SwapChainD3D12 : public SwapChain {
public:
    SwapChainD3D12(Ref<class DeviceD3D12> device, Ref<class QueueD3D12> queue, uint32_t width, uint32_t height);
    ~SwapChainD3D12() override;

    void Resize(uint32_t width, uint32_t height) override;

    bool AcquireNextTexture(Ref<Semaphore> acquired_semaphore) override;

    Ref<Texture> GetCurrentTexture() override { return textures_[curr_texture_].AsRef(); }

private:
    void CreateSwapChainTexture();

    Ref<DeviceD3D12> device_;
    Ref<QueueD3D12> queue_;

    ComPtr<IDXGISwapChain3> swap_chain_;

    uint32_t width_;
    uint32_t height_;

    uint32_t curr_texture_ = 0;
    Vec<Ptr<TextureD3D12>> textures_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

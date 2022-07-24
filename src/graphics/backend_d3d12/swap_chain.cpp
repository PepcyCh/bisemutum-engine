#include "swap_chain.hpp"

#include "device.hpp"
#include "queue.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

SwapChainD3D12::SwapChainD3D12(DeviceD3D12 *device, QueueD3D12 *queue, uint32_t width, uint32_t height) {
    device_ = device;
    queue_ = queue;

    CreateSwapChain(width, height);
}

SwapChainD3D12::~SwapChainD3D12() {}

void SwapChainD3D12::CreateSwapChain(uint32_t width, uint32_t height) {
    swap_chain_.Reset();

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc {
        .Width = width,
        .Height = height,
        .Format = device_->RawSurfaceFormat(),
        .Stereo = false,
        .SampleDesc = { .Count = 1, .Quality = 0 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = kNumSwapChainBuffers,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
    };
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC swap_chain_fullscreen_desc {
        .RefreshRate = { 60, 1 },
        .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
        .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
        .Windowed = true,
    };

    IDXGISwapChain1 *swap_chain_temp;
    device_->RawFactory()->CreateSwapChainForHwnd(queue_->Raw(), device_->RawWindow(), &swap_chain_desc,
        &swap_chain_fullscreen_desc, nullptr, &swap_chain_temp);
    
    device_->RawFactory()->MakeWindowAssociation(device_->RawWindow(), DXGI_MWA_NO_ALT_ENTER);

    swap_chain_temp->QueryInterface(IID_PPV_ARGS(&swap_chain_));
    swap_chain_temp->Release();
    swap_chain_temp = nullptr;

    TextureDesc texture_desc {
        .name = "",
        .extent = { width, height },
        .levels = 1,
        .format = device_->SurfaceFormat(),
        .dim = TextureDimension::e2D,
        .usages = TextureUsage::eColorAttachment,
    };
    textures_.resize(kNumSwapChainBuffers);
    for (uint32_t i = 0; i < kNumSwapChainBuffers; i++) {
        ComPtr<ID3D12Resource> buffer;
        swap_chain_->GetBuffer(i, IID_PPV_ARGS(&buffer));
        textures_[i] = std::make_unique<TextureD3D12>(device_, std::move(buffer), texture_desc);
    }
}

bool SwapChainD3D12::AcquireNextTexture(Semaphore *acquired_semaphore) {
    curr_texture_ = swap_chain_->GetCurrentBackBufferIndex();
    return true;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

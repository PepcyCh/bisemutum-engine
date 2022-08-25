#include "swap_chain.hpp"

#include "device.hpp"
#include "queue.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

DXGI_FORMAT FormatRemoveSrgb(DXGI_FORMAT format) {
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    } else if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    } else {
        return format;
    }
}

}

SwapChainD3D12::SwapChainD3D12(Ref<DeviceD3D12> device, Ref<QueueD3D12> queue, uint32_t width, uint32_t height)
    : device_(device), queue_(queue), width_(width), height_(height) {
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc {
        .Width = width,
        .Height = height,
        .Format = FormatRemoveSrgb(device_->RawSurfaceFormat()),
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
        .RefreshRate = { .Numerator = 60, .Denominator = 1 },
        .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
        .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
        .Windowed = true,
    };

    ComPtr<IDXGISwapChain1> swap_chain_temp;
    device_->RawFactory()->CreateSwapChainForHwnd(queue_->Raw(), device_->RawWindow(), &swap_chain_desc,
        &swap_chain_fullscreen_desc, nullptr, &swap_chain_temp);
    
    device_->RawFactory()->MakeWindowAssociation(device_->RawWindow(), DXGI_MWA_NO_ALT_ENTER);

    swap_chain_temp->QueryInterface(IID_PPV_ARGS(&swap_chain_));

    CreateSwapChainTexture();

    BI_INFO(gGraphicsLogger, "Swapchain created with {} textures, {} x {}", kNumSwapChainBuffers, width_, height_);
}

SwapChainD3D12::~SwapChainD3D12() {}

void SwapChainD3D12::CreateSwapChainTexture() {
    TextureDesc texture_desc {
        .name = "",
        .extent = { width_, height_ },
        .levels = 1,
        .format = device_->SurfaceFormat(),
        .dim = TextureDimension::e2D,
        .usages = TextureUsage::eColorAttachment,
    };
    textures_.resize(kNumSwapChainBuffers);
    for (uint32_t i = 0; i < kNumSwapChainBuffers; i++) {
        ComPtr<ID3D12Resource> buffer;
        swap_chain_->GetBuffer(i, IID_PPV_ARGS(&buffer));
        textures_[i] = Ptr<TextureD3D12>::Make(device_, std::move(buffer), texture_desc);
    }
}

void SwapChainD3D12::Resize(uint32_t width, uint32_t height) {
    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        textures_.clear();
        swap_chain_->ResizeBuffers(kNumSwapChainBuffers, width, height,
            FormatRemoveSrgb(device_->RawSurfaceFormat()), DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
        CreateSwapChainTexture();

        BI_INFO(gGraphicsLogger, "Swapchain resized to {} x {} with {} textures", width_, height_, kNumSwapChainBuffers);
    }
}

bool SwapChainD3D12::AcquireNextTexture(Ref<Semaphore> acquired_semaphore) {
    curr_texture_ = swap_chain_->GetCurrentBackBufferIndex();
    return true;
}

void SwapChainD3D12::Present(Span<Ref<Semaphore>> wait_semaphores) {
    swap_chain_->Present(0, 0);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

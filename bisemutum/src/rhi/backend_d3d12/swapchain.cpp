#include "swapchain.hpp"

#include <bisemutum/prelude/misc.hpp>

#include "device.hpp"

namespace bi::rhi {

namespace {

constexpr uint32_t num_swapchain_buffers = 3;

auto format_remove_srgb(DXGI_FORMAT format) -> DXGI_FORMAT {
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    } else if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    } else {
        return format;
    }
}

auto from_dx_format(DXGI_FORMAT format) -> ResourceFormat {
    switch (format) {
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return ResourceFormat::bgra8_srgb;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return ResourceFormat::rgba8_srgb;
        case DXGI_FORMAT_B8G8R8A8_UNORM: return ResourceFormat::bgra8_unorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM: return ResourceFormat::rgba8_unorm;
        default: unreachable();
    }
}

auto to_dx_usage(BitFlags<TextureUsage> usage) -> DXGI_USAGE {
    DXGI_USAGE dx_usage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    if (usage.contains(TextureUsage::storage_read_write)) {
        dx_usage |= DXGI_USAGE_UNORDERED_ACCESS;
    }
    return dx_usage;
}

}

SwapchainD3D12::SwapchainD3D12(Ref<DeviceD3D12> device, SwapchainDesc const& desc)
    : device_(device), queue_(desc.queue.cast_to<QueueD3D12>()), width_(desc.width), height_(desc.height)
{
    constexpr DXGI_FORMAT surface_formats[]{
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
    };
    surface_format_ = surface_formats[0];

    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{
        .Width = desc.width,
        .Height = desc.height,
        .Format = format_remove_srgb(surface_format_),
        .Stereo = false,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = num_swapchain_buffers,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
    };
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapchain_fullscreen_desc{
        .RefreshRate = {.Numerator = 60, .Denominator = 1},
        .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
        .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
        .Windowed = true,
    };

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain_temp;
    device_->raw_factory()->CreateSwapChainForHwnd(
        queue_->raw(), reinterpret_cast<HWND>(desc.window_handle.win32_window),
        &swapchain_desc, &swapchain_fullscreen_desc, nullptr,
        &swapchain_temp
    );
    
    device_->raw_factory()->MakeWindowAssociation(
        reinterpret_cast<HWND>(desc.window_handle.win32_window), DXGI_MWA_NO_ALT_ENTER
    );

    swapchain_temp->QueryInterface(IID_PPV_ARGS(&swapchain_));

    create_swapchain_textures();
}

auto SwapchainD3D12::create_swapchain_textures() -> void {
    TextureDesc texture_desc{
        .extent = {width_, height_},
        .levels = 1,
        .format = from_dx_format(surface_format_),
        .dim = TextureDimension::d2,
        .usages = TextureUsage::color_attachment,
    };
    textures_.resize(num_swapchain_buffers);
    for (size_t i = 0; i < textures_.size(); i++) {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        swapchain_->GetBuffer(i, IID_PPV_ARGS(&buffer));
        textures_[i] = Box<TextureD3D12>::make(device_, std::move(buffer), texture_desc);
    }
}

auto SwapchainD3D12::resize(uint32_t width, uint32_t height) -> void {
    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        textures_.clear();
        swapchain_->ResizeBuffers(
            num_swapchain_buffers, width, height,
            format_remove_srgb(surface_format_), DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        );
        create_swapchain_textures();
    }
}

auto SwapchainD3D12::acquire_next_texture(Ref<Semaphore> acquired_semaphore) -> bool {
    curr_texture_ = swapchain_->GetCurrentBackBufferIndex();
    return true;
}

auto SwapchainD3D12::current_texture() -> Ref<Texture> {
    return textures_[curr_texture_].ref();
}

auto SwapchainD3D12::present(CSpan<Ref<Semaphore>> wait_semaphores) -> void {
    swapchain_->Present(0, 0);
}

}

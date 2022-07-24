#pragma once

#include <D3D12MemAlloc.h>

#include "utils.hpp"
#include "graphics/device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class DeviceD3D12 : public Device {
public:
    DeviceD3D12(const DeviceDesc &desc);
    ~DeviceD3D12() override;

    static Ptr<DeviceD3D12> Create(const DeviceDesc &desc);

    Ptr<Queue> GetQueue(QueueType type) override;

    Ptr<Buffer> CreateBuffer(const BufferDesc &desc) override;

    Ptr<Texture> CreateTexture(const TextureDesc &desc) override;

    Ptr<Sampler> CreateSampler(const SamplerDesc &desc) override;

    ID3D12Device2 *Raw() const { return device_.Get(); }

    IDXGIFactory6 *RawFactory() const { return factory_.Get(); }

    D3D12MA::Allocator *RawAllocator() const { return allocator_; }

    HWND RawWindow() const { return window_; }
    DXGI_FORMAT RawSurfaceFormat() const { return raw_surface_format_; }
    ResourceFormat SurfaceFormat() const { return surface_format_; }

private:
    ComPtr<ID3D12Debug3> debug_;
    ComPtr<IDXGIFactory6> factory_;
    ComPtr<ID3D12Device2> device_;

    D3D12MA::Allocator *allocator_;

    HWND window_;
    ResourceFormat surface_format_;
    DXGI_FORMAT raw_surface_format_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

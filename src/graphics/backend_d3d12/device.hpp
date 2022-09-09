#pragma once

#include <D3D12MemAlloc.h>

#include "graphics/device.hpp"
#include "descriptor.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class DeviceD3D12 : public Device, public RefFromThis<DeviceD3D12> {
public:
    DeviceD3D12(const DeviceDesc &desc);
    ~DeviceD3D12() override;

    static Ptr<DeviceD3D12> Create(const DeviceDesc &desc);

    Ptr<Queue> GetQueue(QueueType type) override;

    Ptr<SwapChain> CreateSwapChain(const SwapChainDesc &desc) override;

    Ptr<Fence> CreateFence() override;

    Ptr<Semaphore> CreateSemaphore() override;

    Ptr<Buffer> CreateBuffer(const BufferDesc &desc) override;

    Ptr<Texture> CreateTexture(const TextureDesc &desc) override;

    Ptr<Sampler> CreateSampler(const SamplerDesc &desc) override;

    Ptr<ShaderModule> CreateShaderModule(const Vec<uint8_t> &src_bytes) override;

    Ptr<RenderPipeline> CreateRenderPipeline(const RenderPipelineDesc &desc) override;

    Ptr<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc &desc) override;

    Ptr<FrameContext> CreateFrameContext() override;

    ID3D12Device2 *Raw() const { return device_.Get(); }

    IDXGIFactory6 *RawFactory() const { return factory_.Get(); }

    D3D12MA::Allocator *RawAllocator() const { return allocator_; }

    HWND RawWindow() const { return window_; }
    DXGI_FORMAT RawSurfaceFormat() const { return raw_surface_format_; }
    ResourceFormat SurfaceFormat() const { return surface_format_; }

    Ref<DescriptorHeapD3D12> RtvHeap() const { return rtv_heap_.AsRef(); }
    Ref<DescriptorHeapD3D12> DsvHeap() const { return dsv_heap_.AsRef(); }
    Ref<DescriptorHeapD3D12> CbvSrvUavHeap() const { return cbv_srv_uav_heap_.AsRef(); }
    Ref<DescriptorHeapD3D12> SamplerHeap() const { return sampler_heap_.AsRef(); }
    Ref<DescriptorHeapD3D12> Heap(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

private:
    void CreateDescriptorHeap();

    ComPtr<ID3D12Debug3> debug_;
    ComPtr<IDXGIFactory6> factory_;
    ComPtr<ID3D12Device2> device_;
    ComPtr<ID3D12InfoQueue> info_queue_;

    D3D12MA::Allocator *allocator_;

    HWND window_;
    ResourceFormat surface_format_;
    DXGI_FORMAT raw_surface_format_;

    Ptr<DescriptorHeapD3D12> rtv_heap_;
    Ptr<DescriptorHeapD3D12> dsv_heap_;
    Ptr<DescriptorHeapD3D12> cbv_srv_uav_heap_;
    Ptr<DescriptorHeapD3D12> sampler_heap_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

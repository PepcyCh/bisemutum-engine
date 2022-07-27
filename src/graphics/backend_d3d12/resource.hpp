#pragma once

#include <D3D12MemAlloc.h>

#include "core/container.hpp"
#include "utils.hpp"
#include "graphics/resource.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class BufferD3D12 : public Buffer {
public:
    BufferD3D12(Ref<class DeviceD3D12> device, const BufferDesc &desc);
    ~BufferD3D12() override;

    uint64_t Size() const { return size_; }

    ID3D12Resource *Raw() const { return resource_.Get(); }

private:
    Ref<DeviceD3D12> device_;
    ComPtr<ID3D12Resource> resource_;
    D3D12MA::Allocation *allocation_ = nullptr;
    uint64_t size_;

    void *mapped_ptr_ = nullptr;
};

class TextureD3D12 : public Texture {
public:
    TextureD3D12(Ref<class DeviceD3D12> device, const TextureDesc &desc);
    // external image
    TextureD3D12(Ref<class DeviceD3D12> device, ComPtr<ID3D12Resource> &&raw_resource, const TextureDesc &desc);
    ~TextureD3D12() override;

    void GetDepthAndLayer(uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers) const;

    UINT SubresourceIndex(uint32_t level, uint32_t layer, uint32_t plane = 0) const;

    const TextureDesc &Desc() const { return desc_; }

    ID3D12Resource *Raw() const { return resource_.Get(); }

private:
    Ref<DeviceD3D12> device_;
    ComPtr<ID3D12Resource> resource_;
    D3D12MA::Allocation *allocation_ = nullptr;
    TextureDesc desc_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

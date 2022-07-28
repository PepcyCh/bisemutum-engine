#pragma once

#include <D3D12MemAlloc.h>

#include "core/container.hpp"
#include "graphics/resource.hpp"
#include "descriptor.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

enum class BufferViewTypeD3D12 : uint8_t {
    eCbv,
    eSrv,
    eUav,
};

struct BufferViewD3D12Desc {
    uint64_t offset = 0;
    uint64_t size = 0;
    BufferViewTypeD3D12 type = BufferViewTypeD3D12::eCbv;

    bool operator==(const BufferViewD3D12Desc &rhs) const = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

template<>
struct std::hash<bismuth::gfx::BufferViewD3D12Desc> {
    size_t operator()(const bismuth::gfx::BufferViewD3D12Desc &v) const noexcept {
        return bismuth::Hash(v.offset, v.size, v.type);
    }
};

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class BufferD3D12 : public Buffer {
public:
    BufferD3D12(Ref<class DeviceD3D12> device, const BufferDesc &desc);
    ~BufferD3D12() override;

    uint64_t Size() const { return size_; }

    DescriptorHandle GetView(const BufferViewD3D12Desc &view_desc) const;

    ID3D12Resource *Raw() const { return resource_.Get(); }

private:
    Ref<DeviceD3D12> device_;
    ComPtr<ID3D12Resource> resource_;
    D3D12MA::Allocation *allocation_ = nullptr;
    uint64_t size_;

    void *mapped_ptr_ = nullptr;

    mutable HashMap<BufferViewD3D12Desc, DescriptorHandle> cached_views_;
};

enum class TextureViewTypeD3D12 : uint8_t {
    eSrv,
    eUav,
    eRtv,
    eDsv,
};

struct TextureViewD3D12Desc {
    uint32_t base_layer = 0;
    uint32_t layers = 0;
    uint32_t base_level = 0;
    uint32_t levels = 0;
    TextureViewTypeD3D12 type = TextureViewTypeD3D12::eSrv;

    bool operator==(const TextureViewD3D12Desc &rhs) const = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

template<>
struct std::hash<bismuth::gfx::TextureViewD3D12Desc> {
    size_t operator()(const bismuth::gfx::TextureViewD3D12Desc &v) const noexcept {
        return bismuth::Hash(v.base_layer, v.layers, v.base_level, v.levels, v.type);
    }
};

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class TextureD3D12 : public Texture {
public:
    TextureD3D12(Ref<class DeviceD3D12> device, const TextureDesc &desc);
    // external image
    TextureD3D12(Ref<class DeviceD3D12> device, ComPtr<ID3D12Resource> &&raw_resource, const TextureDesc &desc);
    ~TextureD3D12() override;

    void GetDepthAndLayer(uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers) const;

    UINT SubresourceIndex(uint32_t level, uint32_t layer, uint32_t plane = 0) const;

    DescriptorHandle GetView(const TextureViewD3D12Desc &view_desc) const;

    const TextureDesc &Desc() const { return desc_; }

    ID3D12Resource *Raw() const { return resource_.Get(); }

private:
    Ref<DeviceD3D12> device_;
    ComPtr<ID3D12Resource> resource_;
    D3D12MA::Allocation *allocation_ = nullptr;
    TextureDesc desc_;

    mutable HashMap<TextureViewD3D12Desc, DescriptorHandle> cached_views_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

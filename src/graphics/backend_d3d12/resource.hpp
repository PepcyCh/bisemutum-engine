#pragma once

#include <D3D12MemAlloc.h>

#include "core/container.hpp"
#include "graphics/resource.hpp"
#include "descriptor.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct BufferViewD3D12Desc {
    uint64_t offset = 0;
    uint64_t size = 0;
    DescriptorType descriptor_type = DescriptorType::eNone;
    uint32_t struct_stride = 0;

    bool operator==(const BufferViewD3D12Desc &rhs) const = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

template<>
struct std::hash<bismuth::gfx::BufferViewD3D12Desc> {
    size_t operator()(const bismuth::gfx::BufferViewD3D12Desc &v) const noexcept {
        return bismuth::Hash(v.offset, v.size, v.descriptor_type, v.struct_stride);
    }
};

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class BufferD3D12 : public Buffer {
public:
    BufferD3D12(Ref<class DeviceD3D12> device, const BufferDesc &desc);
    ~BufferD3D12() override;

    void *Map() override;

    void Unmap() override;

    uint64_t Size() const { return size_; }

    bool IsStateRestricted() const { return state_restricted_; }

    DescriptorHandle GetView(const BufferViewD3D12Desc &view_desc) const;

    ID3D12Resource *Raw() const { return resource_.Get(); }

private:
    Ref<DeviceD3D12> device_;
    ComPtr<ID3D12Resource> resource_;
    D3D12MA::Allocation *allocation_ = nullptr;
    uint64_t size_;
    bool state_restricted_ = false;

    void *mapped_ptr_ = nullptr;

    mutable HashMap<BufferViewD3D12Desc, DescriptorHandle> cached_views_;
};

struct TextureViewD3D12Desc {
    uint32_t base_layer = 0;
    uint32_t layers = 0;
    uint32_t base_level = 0;
    uint32_t levels = 0;
    DescriptorType descriptor_type = DescriptorType::eNone;
    TextureViewDimension dim = TextureViewDimension::e1D;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    bool operator==(const TextureViewD3D12Desc &rhs) const = default;
};

struct TextureRenderTargetViewD3D12Desc {
    uint32_t base_layer = 0;
    uint32_t layers = 0;
    uint32_t base_level = 0;
    uint32_t levels = 0;

    bool operator==(const TextureRenderTargetViewD3D12Desc &rhs) const = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

template<>
struct std::hash<bismuth::gfx::TextureViewD3D12Desc> {
    size_t operator()(const bismuth::gfx::TextureViewD3D12Desc &v) const noexcept {
        return bismuth::Hash(v.base_layer, v.layers, v.base_level, v.levels, v.descriptor_type, v.dim, v.format);
    }
};

template<>
struct std::hash<bismuth::gfx::TextureRenderTargetViewD3D12Desc> {
    size_t operator()(const bismuth::gfx::TextureRenderTargetViewD3D12Desc &v) const noexcept {
        return bismuth::Hash(v.base_layer, v.layers, v.base_level, v.levels);
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

    void GetDepthAndLayer(uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers, uint32_t another = 1) const;
    uint32_t Layers() const;

    UINT SubresourceIndex(uint32_t level, uint32_t layer, uint32_t plane = 0) const;

    bool IsStateRestricted() const { return state_restricted_; }

    DescriptorHandle GetView(const TextureViewD3D12Desc &view_desc) const;
    DescriptorHandle GetView(const TextureRenderTargetViewD3D12Desc &view_desc) const;

    const TextureDesc &Desc() const { return desc_; }

    ID3D12Resource *Raw() const { return resource_.Get(); }

    DXGI_FORMAT RawFormat() const;

private:
    Ref<DeviceD3D12> device_;
    ComPtr<ID3D12Resource> resource_;
    D3D12MA::Allocation *allocation_ = nullptr;
    TextureDesc desc_;
    bool state_restricted_ = false;

    mutable HashMap<TextureViewD3D12Desc, DescriptorHandle> cached_views_;
    mutable HashMap<TextureRenderTargetViewD3D12Desc, DescriptorHandle> cached_render_target_views_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

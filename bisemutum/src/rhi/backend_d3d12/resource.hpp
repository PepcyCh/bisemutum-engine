#pragma once

#include <unordered_map>

#include <bisemutum/rhi/resource.hpp>
#include <bisemutum/prelude/ref.hpp>
#include <d3d12.h>
#include <D3D12MemAlloc.h>
#include <wrl.h>

namespace bi::rhi {

struct DeviceD3D12;

struct BufferD3D12 final : Buffer {
    BufferD3D12(Ref<DeviceD3D12> device, BufferDesc const& desc);
    ~BufferD3D12() override;

    auto map() -> void* override;

    auto unmap() -> void override;

    auto size() const -> uint64_t { return desc_.size; }

    auto is_state_restricted() const -> bool { return state_restricted_; }

    auto raw() const -> ID3D12Resource* { return resource_.Get(); }

    auto get_current_state() const -> D3D12_RESOURCE_STATES { return current_state_; }
    auto set_current_state(D3D12_RESOURCE_STATES state) -> void { current_state_ = state; }

private:
    Ref<DeviceD3D12> device_;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_ = nullptr;
    D3D12MA::Allocation* allocation_ = nullptr;
    bool state_restricted_ = false;

    D3D12_RESOURCE_STATES current_state_ = D3D12_RESOURCE_STATE_COMMON;

    void* mapped_ptr_ = nullptr;
};


struct TextureRenderTargetViewKeyD3D12 final {
    uint32_t level;
    uint32_t base_layer;
    uint32_t num_layers;
    DXGI_FORMAT format;
    TextureViewType view_type;

    auto operator==(TextureRenderTargetViewKeyD3D12 const& rhs) const -> bool = default;
};

}

template <>
struct std::hash<bi::rhi::TextureRenderTargetViewKeyD3D12> final {
    auto operator()(bi::rhi::TextureRenderTargetViewKeyD3D12 const& v) const noexcept -> size_t;
};

namespace bi::rhi {

struct TextureD3D12 final : Texture {
    TextureD3D12(Ref<DeviceD3D12> device, TextureDesc const& desc);
    // external image
    TextureD3D12(Ref<DeviceD3D12> device, Microsoft::WRL::ComPtr<ID3D12Resource>&& raw_resource, TextureDesc const& desc);
    ~TextureD3D12() override;

    auto get_depth_and_layer(
        uint32_t depth_or_layers, uint32_t& depth, uint32_t& layers, uint32_t another = 1
    ) const -> void;

    auto layers() const -> uint32_t;

    auto subresource_index(uint32_t level, uint32_t layer, uint32_t plane = 0) const -> uint32_t;

    auto raw() const -> ID3D12Resource* { return resource_.Get(); }

    auto raw_format() const -> DXGI_FORMAT;

    auto get_render_target_view(
        uint32_t level, uint32_t base_layer, uint32_t num_layers, DXGI_FORMAT format, TextureViewType view_type
    ) -> uint64_t;

    auto get_current_state() const -> D3D12_RESOURCE_STATES { return current_state_; }
    auto set_current_state(D3D12_RESOURCE_STATES state) -> void { current_state_ = state; }

private:
    Ref<DeviceD3D12> device_;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_ = nullptr;
    D3D12MA::Allocation* allocation_ = nullptr;

    D3D12_RESOURCE_STATES current_state_ = D3D12_RESOURCE_STATE_COMMON;

    std::unordered_map<TextureRenderTargetViewKeyD3D12, uint64_t> rtv_dsv_;
};

}

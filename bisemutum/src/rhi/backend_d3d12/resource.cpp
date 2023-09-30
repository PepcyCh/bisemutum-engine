#include "resource.hpp"

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>

#include "device.hpp"
#include "utils.hpp"

auto std::hash<bi::rhi::TextureRenderTargetViewKeyD3D12>::operator()(
    bi::rhi::TextureRenderTargetViewKeyD3D12 const& v
) const noexcept -> size_t {
    return bi::hash(v.level, v.base_layer, v.num_layers, v.format, v.view_type);
}

namespace bi::rhi {

namespace {

auto to_dx_dimension(TextureDimension dim) -> D3D12_RESOURCE_DIMENSION {
    switch (dim) {
        case TextureDimension::d1: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        case TextureDimension::d2: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        case TextureDimension::d3: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }
    unreachable();
}

auto to_dx_resource_flags(BitFlags<BufferUsage> usage) -> D3D12_RESOURCE_FLAGS {
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (usage.contains(BufferUsage::storage_read_write)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    return flags;
}

auto to_dx_resource_flags(BitFlags<TextureUsage> usage) -> D3D12_RESOURCE_FLAGS {
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (usage.contains(TextureUsage::storage_read_write)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (usage.contains(TextureUsage::color_attachment)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (usage.contains(TextureUsage::depth_stencil_attachment)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    return flags;
}

auto to_dx_heap_type(BufferMemoryProperty prop) -> D3D12_HEAP_TYPE {
    switch (prop) {
        case BufferMemoryProperty::gpu_only: return D3D12_HEAP_TYPE_DEFAULT;
        case BufferMemoryProperty::cpu_to_gpu: return D3D12_HEAP_TYPE_UPLOAD;
        case BufferMemoryProperty::gpu_to_cpu: return D3D12_HEAP_TYPE_READBACK;
    }
    unreachable();
}

}

BufferD3D12::BufferD3D12(Ref<DeviceD3D12> device, const BufferDesc &desc) : device_(device) {
    desc_ = desc;
    if (desc.usages.contains(BufferUsage::uniform)) {
        desc_.size = (desc.size + 255) >> 8 << 8;
        desc_.size = aligned_size<uint64_t>(desc_.size, 256);
    }

    D3D12_RESOURCE_DESC resource_desc{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        .Width = desc_.size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = to_dx_resource_flags(desc.usages),
    };
    D3D12MA::ALLOCATION_DESC allocation_desc{
        .HeapType = to_dx_heap_type(desc_.memory_property),
    };
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
    if (desc_.memory_property == BufferMemoryProperty::cpu_to_gpu) {
        initial_state = D3D12_RESOURCE_STATE_GENERIC_READ;
        state_restricted_ = true;
    } else if (desc_.memory_property == BufferMemoryProperty::gpu_to_cpu) {
        initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
        state_restricted_ = true;
    }
    device_->allocator()->CreateResource(
        &allocation_desc, &resource_desc, initial_state, nullptr, &allocation_, IID_PPV_ARGS(&resource_)
    );

    if (desc_.persistently_mapped) {
        resource_->Map(0, nullptr, &mapped_ptr_);
    }
}

BufferD3D12::~BufferD3D12() {
    if (allocation_) {
        unmap();
        allocation_->Release();
        allocation_ = nullptr;
    }
}

auto BufferD3D12::map() -> void* {
    if (mapped_ptr_ == nullptr) {
        resource_->Map(0, nullptr, &mapped_ptr_);
    }
    return mapped_ptr_;
}

void BufferD3D12::unmap() {
    if (mapped_ptr_ && !desc_.persistently_mapped) {
        resource_->Unmap(0, nullptr);
        mapped_ptr_ = nullptr;
    }
}

TextureD3D12::TextureD3D12(Ref<DeviceD3D12> device, TextureDesc const& desc) : device_(device) {
    desc_ = desc;
    D3D12_RESOURCE_DESC resource_desc{
        .Dimension = to_dx_dimension(desc.dim),
        .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        .Width = desc.extent.width,
        .Height = desc.extent.height,
        .DepthOrArraySize = static_cast<UINT16>(desc.extent.depth_or_layers),
        .MipLevels = static_cast<UINT16>(desc.levels),
        .Format = to_dx_format(desc.format),
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = to_dx_resource_flags(desc.usages),
    };
    D3D12MA::ALLOCATION_DESC allocation_desc{
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
    device_->allocator()->CreateResource(
        &allocation_desc, &resource_desc, initial_state, nullptr, &allocation_, IID_PPV_ARGS(&resource_)
    );
}

TextureD3D12::TextureD3D12(
    Ref<DeviceD3D12> device, Microsoft::WRL::ComPtr<ID3D12Resource>&& raw_resource, TextureDesc const& desc
)
    : device_(device)
{
    desc_ = desc;
    resource_ = std::move(raw_resource);
}

TextureD3D12::~TextureD3D12() {
    if (allocation_) {
        allocation_->Release();
        allocation_ = nullptr;
    }
}

auto TextureD3D12::get_depth_and_layer(
    uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers, uint32_t another
) const -> void {
    if (desc_.dim == TextureDimension::d3) {
        depth = depth_or_layers;
        layers = another;
    } else {
        depth = another;
        layers = depth_or_layers;
    }
}

auto TextureD3D12::layers() const -> uint32_t {
    return desc_.dim == TextureDimension::d3 ? 1 : desc_.extent.depth_or_layers;
}

auto TextureD3D12::subresource_index(uint32_t level, uint32_t layer, uint32_t plane) const -> uint32_t {
    uint32_t layers = desc_.dim == TextureDimension::d3 ? 1 : desc_.extent.depth_or_layers;
    return level + layer * desc_.levels + plane * layers * desc_.levels;
}

auto TextureD3D12::raw_format() const -> DXGI_FORMAT {
    return to_dx_format(desc_.format);
}

auto TextureD3D12::get_render_target_view(
    uint32_t level, uint32_t base_layer, uint32_t num_layers, DXGI_FORMAT format, TextureViewType view_type
) -> uint64_t {
    auto [view_it, create] = rtv_dsv_.try_emplace({level, base_layer, num_layers, format, view_type});
    if (create) {
        auto is_color = is_color_format(desc_.format);
        auto heap = is_color ? device_->rtv_heap() : device_->dsv_heap();
        auto descriptor = heap->allocate_descriptor_single();
        if (is_color) {
            D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{
                .Format = format,
            };
            if (view_type == TextureViewType::automatic) {
                view_type = get_automatic_view_type();
            }
            switch (view_type) {
                case TextureViewType::d1:
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                    rtv_desc.Texture1D.MipSlice = level;
                    break;
                case TextureViewType::d1_array:
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                    rtv_desc.Texture1DArray.MipSlice = level;
                    rtv_desc.Texture1DArray.FirstArraySlice = base_layer;
                    rtv_desc.Texture1DArray.ArraySize = num_layers;
                    break;
                case TextureViewType::d2:
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                    rtv_desc.Texture2D.MipSlice = level;
                    rtv_desc.Texture2D.PlaneSlice = 0;
                    break;
                case TextureViewType::d2_array:
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    rtv_desc.Texture2DArray.MipSlice = level;
                    rtv_desc.Texture2DArray.FirstArraySlice = base_layer;
                    rtv_desc.Texture2DArray.ArraySize = num_layers;
                    rtv_desc.Texture2DArray.PlaneSlice = 0;
                    break;
                case TextureViewType::d3:
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                    rtv_desc.Texture3D.MipSlice = level;
                    rtv_desc.Texture3D.FirstWSlice = base_layer;
                    rtv_desc.Texture3D.WSize = num_layers;
                    break;
                default: unreachable();
            }
            device_->raw()->CreateRenderTargetView(resource_.Get(), &rtv_desc, {descriptor.cpu});
        } else {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{
                .Format = format,
                .Flags = D3D12_DSV_FLAG_NONE,
            };
            if (view_type == TextureViewType::automatic) {
                view_type = get_automatic_view_type();
            }
            switch (view_type) {
                case TextureViewType::d1:
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                    dsv_desc.Texture1D.MipSlice = level;
                    break;
                case TextureViewType::d1_array:
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                    dsv_desc.Texture1DArray.MipSlice = level;
                    dsv_desc.Texture1DArray.FirstArraySlice = base_layer;
                    dsv_desc.Texture1DArray.ArraySize = num_layers;
                    break;
                case TextureViewType::d2:
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                    dsv_desc.Texture2D.MipSlice = level;
                    break;
                case TextureViewType::d2_array:
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                    dsv_desc.Texture2DArray.MipSlice = level;
                    dsv_desc.Texture2DArray.FirstArraySlice = base_layer;
                    dsv_desc.Texture2DArray.ArraySize = num_layers;
                    break;
                default: unreachable();
            }
            device_->raw()->CreateDepthStencilView(resource_.Get(), &dsv_desc, {descriptor.cpu});
        }
        view_it->second = descriptor.cpu;
    }
    return view_it->second;
}

}

#include "resource.hpp"

#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

D3D12_RESOURCE_DIMENSION ToDxDimension(TextureDimension dim) {
    switch (dim) {
        case TextureDimension::e1D: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        case TextureDimension::e2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        case TextureDimension::e3D: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }
    Unreachable();
}

D3D12_RESOURCE_FLAGS ToDxResourceFlags(BitFlags<BufferUsage> usage) {
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (usage.Contains(BufferUsage::eUnorderedAccess) || usage.Contains(BufferUsage::eAccelerationStructure)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    return flags;
}

D3D12_RESOURCE_FLAGS ToDxResourceFlags(BitFlags<TextureUsage> usage) {
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (usage.Contains(TextureUsage::eUnorderedAccess)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (usage.Contains(TextureUsage::eColorAttachment)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (usage.Contains(TextureUsage::eDepthStencilAttachment)) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    return flags;
}

D3D12_HEAP_TYPE ToDxHeapType(BufferMemoryProperty prop) {
    switch (prop) {
        case BufferMemoryProperty::eGpuOnly: return D3D12_HEAP_TYPE_DEFAULT;
        case BufferMemoryProperty::eCpuToGpu: return D3D12_HEAP_TYPE_UPLOAD;
        case BufferMemoryProperty::eGpuToCpu: return D3D12_HEAP_TYPE_READBACK;
    }
    Unreachable();
}

D3D12_DESCRIPTOR_HEAP_TYPE ToDescriptorHeapType(TextureViewTypeD3D12 type) {
    switch (type) {
        case TextureViewTypeD3D12::eSrv:
        case TextureViewTypeD3D12::eUav:
            return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        case TextureViewTypeD3D12::eRtv:
            return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        case TextureViewTypeD3D12::eDsv:
            return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    }
    Unreachable();
}

}

BufferD3D12::BufferD3D12(Ref<DeviceD3D12> device, const BufferDesc &desc) : device_(device), size_(desc.size) {
    D3D12_RESOURCE_DESC resource_desc {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        .Width = desc.size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = { .Count = 1, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = ToDxResourceFlags(desc.usages),
    };
    D3D12MA::ALLOCATION_DESC allocation_desc {
        .HeapType = ToDxHeapType(desc.memory_property),
    };
    device_->RawAllocator()->CreateResource(&allocation_desc, &resource_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
        &allocation_, IID_PPV_ARGS(&resource_));

    if (desc.persistently_mapped) {
        resource_->Map(0, nullptr, &mapped_ptr_);
    }

    if (!desc.name.empty()) {
        resource_->SetPrivateData(WKPDID_D3DDebugObjectName, desc.name.size(), desc.name.data());
    }
}

BufferD3D12::~BufferD3D12() {
    if (mapped_ptr_) {
        resource_->Unmap(0, nullptr);
        mapped_ptr_ = nullptr;
    }
    if (allocation_) {
        allocation_->Release();
        allocation_ = nullptr;
    }
}

DescriptorHandle BufferD3D12::GetView(const BufferViewD3D12Desc &view_desc) const {
    if (auto it = cached_views_.find(view_desc); it != cached_views_.end()) {
        return it->second;
    }

    auto handle = device_->Heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->AllocateDecriptor();
    cached_views_.insert({view_desc, handle});
    switch (view_desc.type) {
        case BufferViewTypeD3D12::eCbv: {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc {
                .BufferLocation = resource_->GetGPUVirtualAddress() + view_desc.offset,
                .SizeInBytes = static_cast<UINT>(view_desc.size == 0 ? size_ - view_desc.offset : view_desc.size),
            };
            device_->Raw()->CreateConstantBufferView(&cbv_desc, handle.cpu);
            break;
        }
        case BufferViewTypeD3D12::eSrv: {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc {
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_SRV_DIMENSION_BUFFER, // TODO - ray tracing acceleration structure
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Buffer = D3D12_BUFFER_SRV {
                    .FirstElement = view_desc.offset,
                    .NumElements = static_cast<UINT>(view_desc.size == 0 ? size_ - view_desc.offset : view_desc.size),
                    .StructureByteStride = 1,
                    .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
                },
            };
            device_->Raw()->CreateShaderResourceView(resource_.Get(), &srv_desc, handle.cpu);
            break;
        }
        case BufferViewTypeD3D12::eUav: {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc {
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                .Buffer = D3D12_BUFFER_UAV {
                    .FirstElement = view_desc.offset,
                    .NumElements = static_cast<UINT>(view_desc.size == 0 ? size_ - view_desc.offset : view_desc.size),
                    .StructureByteStride = 1,
                    .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
                },
            };
            device_->Raw()->CreateUnorderedAccessView(resource_.Get(), nullptr, &uav_desc, handle.cpu);
            break;
        }
    }
    return handle;
}


TextureD3D12::TextureD3D12(Ref<DeviceD3D12> device, const TextureDesc &desc) : device_(device), desc_(desc) {
    D3D12_RESOURCE_DESC resource_desc {
        .Dimension = ToDxDimension(desc.dim),
        .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        .Width = desc.extent.width,
        .Height = desc.extent.height,
        .DepthOrArraySize = static_cast<UINT16>(desc.extent.depth_or_layers),
        .MipLevels = static_cast<UINT16>(desc.levels),
        .Format = ToDxFormat(desc.format),
        .SampleDesc = { .Count = 1, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = ToDxResourceFlags(desc.usages),
    };
    D3D12MA::ALLOCATION_DESC allocation_desc {
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };
    device_->RawAllocator()->CreateResource(&allocation_desc, &resource_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
        &allocation_, IID_PPV_ARGS(&resource_));

    if (!desc.name.empty()) {
        resource_->SetPrivateData(WKPDID_D3DDebugObjectName, desc.name.size(), desc.name.data());
    }
}

TextureD3D12::TextureD3D12(Ref<DeviceD3D12> device, ComPtr<ID3D12Resource> &&raw_resource, const TextureDesc &desc)
    : device_(device), desc_(desc) {
    resource_ = raw_resource;
    allocation_ = nullptr;
}

TextureD3D12::~TextureD3D12() {
    if (allocation_) {
        allocation_->Release();
        allocation_ = nullptr;
    }
}

void TextureD3D12::GetDepthAndLayer(uint32_t depth_or_layers, uint32_t &depth, uint32_t &layers) const {
    if (desc_.dim == TextureDimension::e3D) {
        depth = depth_or_layers;
        layers = 1;
    } else {
        depth = 1;
        layers = depth_or_layers;
    }
}

UINT TextureD3D12::SubresourceIndex(uint32_t level, uint32_t layer, uint32_t plane) const {
    uint32_t layers = desc_.dim == TextureDimension::e3D ? 1 : desc_.extent.depth_or_layers;
    return level + layer * desc_.levels + plane * layers * desc_.levels;
}

DescriptorHandle TextureD3D12::GetView(const TextureViewD3D12Desc &view_desc) const {
    if (auto it = cached_views_.find(view_desc); it != cached_views_.end()) {
        return it->second;
    }

    auto handle = device_->Heap(ToDescriptorHeapType(view_desc.type))->AllocateDecriptor();
    cached_views_.insert({view_desc, handle});
    switch (view_desc.type) {
        case TextureViewTypeD3D12::eSrv: {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc {
                .Format = ToDxFormat(desc_.format),
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            };
            switch (desc_.dim) {
                case TextureDimension::e1D:
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                    srv_desc.Texture1D = {
                        .MostDetailedMip = view_desc.base_level,
                        .MipLevels = view_desc.levels == 0 ? -1 : view_desc.levels,
                        .ResourceMinLODClamp = 0.0f,
                    };
                case TextureDimension::e2D:
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srv_desc.Texture2D = {
                        .MostDetailedMip = view_desc.base_level,
                        .MipLevels = view_desc.levels == 0 ? -1 : view_desc.levels,
                        .PlaneSlice = 0,
                        .ResourceMinLODClamp = 0.0f,
                    };
                case TextureDimension::e3D:
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                    srv_desc.Texture3D = {
                        .MostDetailedMip = view_desc.base_level,
                        .MipLevels = view_desc.levels == 0 ? -1 : view_desc.levels,
                        .ResourceMinLODClamp = 0.0f,
                    };
                // case TextureDimension::eCube:
                //     srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                //     srv_desc.TextureCube = {
                //         .MostDetailedMip = view_desc.base_level,
                //         .MipLevels = view_desc.levels == 0 ? -1 : view_desc.levels,
                //         .ResourceMinLODClamp = 0.0f,
                //     };
                // case TextureDimension::e1DArray:
                //     srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                //     srv_desc.Texture1DArray = {
                //         .MostDetailedMip = view_desc.base_level,
                //         .MipLevels = view_desc.levels == 0 ? -1 : view_desc.levels,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .ResourceMinLODClamp = 0.0f,
                //     };
                // case TextureDimension::e2DArray:
                //     srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                //     srv_desc.Texture2DArray = {
                //         .MostDetailedMip = view_desc.base_level,
                //         .MipLevels = view_desc.levels == 0 ? -1 : view_desc.levels,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .PlaneSlice = 0,
                //         .ResourceMinLODClamp = 0.0f,
                //     };
                // case TextureDimension::eCubeArray:
                //     srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                //     srv_desc.TextureCubeArray = {
                //         .MostDetailedMip = view_desc.base_level,
                //         .MipLevels = view_desc.levels == 0 ? -1 : view_desc.levels,
                //         .First2DArrayFace = view_desc.base_layer,
                //         .NumCubes = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .ResourceMinLODClamp = 0.0f,
                //     };
            }
            device_->Raw()->CreateShaderResourceView(resource_.Get(), &srv_desc, handle.cpu);
            break;
        }
        case TextureViewTypeD3D12::eUav: {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc {
                .Format = ToDxFormat(desc_.format),
            };
            switch (desc_.dim) {
                case TextureDimension::e1D:
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                    uav_desc.Texture1D = { .MipSlice = view_desc.base_level };
                case TextureDimension::e2D:
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    uav_desc.Texture2D = { .MipSlice = view_desc.base_level, .PlaneSlice = 0 };
                case TextureDimension::e3D:
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                    uav_desc.Texture3D = {
                        .MipSlice = view_desc.base_level,
                        .FirstWSlice = view_desc.base_layer,
                        .WSize = view_desc.layers == 0 ? -1 : view_desc.layers,
                    };
                // case TextureDimension::eCube:
                //     uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                //     uav_desc.Texture2DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .PlaneSlice = 0,
                //     };
                // case TextureDimension::e1DArray:
                //     uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                //     uav_desc.Texture1DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //     };
                // case TextureDimension::e2DArray:
                //     uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                //     uav_desc.Texture2DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .PlaneSlice = 0,
                //     };
                // case TextureDimension::eCubeArray:
                //     uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                //     uav_desc.Texture2DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .PlaneSlice = 0,
                //     };
            }
            device_->Raw()->CreateUnorderedAccessView(resource_.Get(), nullptr, &uav_desc, handle.cpu);
            break;
        }
        case TextureViewTypeD3D12::eRtv: {
            D3D12_RENDER_TARGET_VIEW_DESC rtv_desc {
                .Format = ToDxFormat(desc_.format),
            };
            switch (desc_.dim) {
                case TextureDimension::e1D:
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                    rtv_desc.Texture1D = { .MipSlice = view_desc.base_level, };
                case TextureDimension::e2D:
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                    rtv_desc.Texture2D = { .MipSlice = view_desc.base_level, .PlaneSlice = 0 };
                case TextureDimension::e3D:
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                    rtv_desc.Texture3D = {
                        .MipSlice = view_desc.base_level,
                        .FirstWSlice = view_desc.base_layer,
                        .WSize = view_desc.layers == 0 ? -1 : view_desc.layers,
                    };
                // case TextureDimension::eCube:
                //     rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                //     rtv_desc.Texture2DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .PlaneSlice = 0,
                //     };
                // case TextureDimension::e1DArray:
                //     rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                //     rtv_desc.Texture1DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //     };
                // case TextureDimension::e2DArray:
                //     rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                //     rtv_desc.Texture2DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .PlaneSlice = 0,
                //     };
                // case TextureDimension::eCubeArray:
                //     rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                //     rtv_desc.Texture2DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //         .PlaneSlice = 0,
                //     };
            }
            device_->Raw()->CreateRenderTargetView(resource_.Get(), &rtv_desc, handle.cpu);
            break;
        }
        case TextureViewTypeD3D12::eDsv: {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc {
                .Format = ToDxFormat(desc_.format),
            };
            switch (desc_.dim) {
                case TextureDimension::e1D:
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                    dsv_desc.Texture1D = { .MipSlice = view_desc.base_level, };
                case TextureDimension::e2D:
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                    dsv_desc.Texture2D = { .MipSlice = view_desc.base_level };
                case TextureDimension::e3D:
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_UNKNOWN;
                // case TextureDimension::eCube:
                //     dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_UNKNOWN;
                // case TextureDimension::e1DArray:
                //     dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                //     dsv_desc.Texture1DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //     };
                // case TextureDimension::e2DArray:
                //     dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                //     dsv_desc.Texture2DArray = {
                //         .MipSlice = view_desc.base_level,
                //         .FirstArraySlice = view_desc.base_layer,
                //         .ArraySize = view_desc.layers == 0 ? -1 : view_desc.layers,
                //     };
                // case TextureDimension::eCubeArray:
                //     dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_UNKNOWN;
            }
            device_->Raw()->CreateDepthStencilView(resource_.Get(), &dsv_desc, handle.cpu);
            break;
        }
    }
    return handle;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

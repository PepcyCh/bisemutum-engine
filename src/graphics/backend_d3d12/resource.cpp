#include "resource.hpp"

#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

D3D12_RESOURCE_DIMENSION ToDxDimension(TextureDimension dim) {
    switch (dim) {
        case TextureDimension::e1D:
        case TextureDimension::e1DArray:
            return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        case TextureDimension::e2D:
        case TextureDimension::e2DArray:
        case TextureDimension::eCube:
        case TextureDimension::eCubeArray:
            return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        case TextureDimension::e3D:
            return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
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

}

BufferD3D12::BufferD3D12(Ref<DeviceD3D12> device, const BufferDesc &desc) : device_(device) {
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

TextureD3D12::TextureD3D12(Ref<DeviceD3D12> device, const TextureDesc &desc) : device_(device) {
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
    : device_(device) {
    resource_ = raw_resource;
    allocation_ = nullptr;
}

TextureD3D12::~TextureD3D12() {
    if (allocation_) {
        allocation_->Release();
        allocation_ = nullptr;
    }
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

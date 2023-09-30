#include "descriptor.hpp"

#include "device.hpp"

namespace bi::rhi {

DescriptorHeapD3D12::DescriptorHeapD3D12(Ref<DeviceD3D12> device, DescriptorHeapDesc const& desc) : device_(device) {
    type_ = desc.type == DescriptorHeapType::sampler
        ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
        : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{
        .Type = type_,
        .NumDescriptors = desc.max_count,
        .Flags = desc.shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    device_->raw()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap_));

    descriptor_size_ = device->raw()->GetDescriptorHandleIncrementSize(type_);
    max_size_ = desc.max_count * descriptor_size_;

    start_handle_.cpu = heap_->GetCPUDescriptorHandleForHeapStart().ptr;
    if (desc.shader_visible) {
        start_handle_.gpu = heap_->GetGPUDescriptorHandleForHeapStart().ptr;
    }
}

DescriptorHeapD3D12::DescriptorHeapD3D12(Ref<DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t max_count)
    : device_(device), type_(type)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{
        .Type = type_,
        .NumDescriptors = max_count,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    device_->raw()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap_));

    descriptor_size_ = device->raw()->GetDescriptorHandleIncrementSize(type_);
    max_size_ = max_count * descriptor_size_;
    start_handle_.cpu = heap_->GetCPUDescriptorHandleForHeapStart().ptr;
}

auto DescriptorHeapD3D12::size_of_descriptor(DescriptorType type) const -> uint32_t {
    return descriptor_size_;
}

auto DescriptorHeapD3D12::allocate_descriptor(DescriptorType type) -> DescriptorHandle {
    return allocate_descriptor_single();
}

auto DescriptorHeapD3D12::allocate_descriptor(BindGroupLayout const& layout) -> DescriptorHandle {
    uint32_t size = 0;
    for (auto const& entry : layout) {
        size += descriptor_size_ * entry.count;
    }
    if (size == 0) { return {}; }
    if (bytes_used_ + size > max_size_) { return {}; }
    DescriptorHandle handle{
        .cpu = start_handle_.cpu + bytes_used_,
        .gpu = start_handle_.gpu ? start_handle_.gpu + bytes_used_ : 0,
    };
    bytes_used_ += size;
    return handle;
}

auto DescriptorHeapD3D12::allocate_descriptor_single() -> DescriptorHandle {
    if (bytes_used_ + descriptor_size_ > max_size_) { return {}; }
    DescriptorHandle handle{
        .cpu = start_handle_.cpu + bytes_used_,
        .gpu = start_handle_.gpu ? start_handle_.gpu + bytes_used_ : 0,
    };
    bytes_used_ += descriptor_size_;
    return handle;
}

}

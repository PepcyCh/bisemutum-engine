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
    max_count_ = desc.max_count;

    start_handle_.cpu = heap_->GetCPUDescriptorHandleForHeapStart().ptr;
    if (desc.shader_visible) {
        start_handle_.gpu = heap_->GetCPUDescriptorHandleForHeapStart().ptr;
    }
}

DescriptorHeapD3D12::DescriptorHeapD3D12(Ref<DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t max_count)
    : device_(device)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{
        .Type = type_,
        .NumDescriptors = max_count,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    device_->raw()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap_));

    descriptor_size_ = device->raw()->GetDescriptorHandleIncrementSize(type_);
    max_count_ = max_count;
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
        size += size_of_descriptor(entry.type) * entry.count;
    }
    if (size == 0) { return {}; }
    if (num_used_ + size > max_count_) { return {}; }
    DescriptorHandle handle{
        .cpu = start_handle_.cpu + descriptor_size_ * num_used_,
        .gpu = start_handle_.gpu ? start_handle_.gpu + descriptor_size_ * num_used_ : 0,
    };
    num_used_ += size;
    return handle;
}

auto DescriptorHeapD3D12::allocate_descriptor_single() -> DescriptorHandle {
    if (num_used_ >= max_count_) { return {}; }
    DescriptorHandle handle{
        .cpu = start_handle_.cpu + descriptor_size_ * num_used_,
        .gpu = start_handle_.gpu ? start_handle_.gpu + descriptor_size_ * num_used_ : 0,
    };
    ++num_used_;
    return handle;
}

}

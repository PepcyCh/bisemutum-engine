#include "descriptor.hpp"

#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

DescriptorHeapD3D12::DescriptorHeapD3D12(Ref<class DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT max_count,
    bool shader_visible)
    : device_(device), type_(type), shader_visible_(shader_visible), max_count_(max_count), used_count_(0) {
    D3D12_DESCRIPTOR_HEAP_DESC desc {
        .Type = type,
        .NumDescriptors = max_count,
        .Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    device->Raw()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));

    descriptor_size_ = device->Raw()->GetDescriptorHandleIncrementSize(type);

    start_handle_ = DescriptorHandle {
        .cpu = heap_->GetCPUDescriptorHandleForHeapStart(),
        .gpu = { .ptr = 0 },
    };
    if (shader_visible) {
        start_handle_.gpu = heap_->GetGPUDescriptorHandleForHeapStart();
    }
}

DescriptorHeapD3D12::~DescriptorHeapD3D12() {}

DescriptorHandle DescriptorHeapD3D12::AllocateDecriptor() {
    BI_ASSERT(used_count_ < max_count_);

    DescriptorHandle handle {
        .cpu = { .ptr = start_handle_.cpu.ptr + used_count_ * descriptor_size_ },
        .gpu = { .ptr = 0 },
    };
    if (shader_visible_) {
        handle.gpu.ptr = start_handle_.gpu.ptr + used_count_ * descriptor_size_ ;
    }
    ++used_count_;

    return handle;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

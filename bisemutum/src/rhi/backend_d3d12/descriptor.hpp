#pragma once

#include <bisemutum/rhi/descriptor.hpp>
#include <d3d12.h>
#include <wrl.h>

namespace bi::rhi {

struct DeviceD3D12;

struct DescriptorHeapD3D12 final : DescriptorHeap {
    DescriptorHeapD3D12(Ref<DeviceD3D12> device, DescriptorHeapDesc const& desc);
    // for RTV & DSV
    DescriptorHeapD3D12(Ref<DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t max_count);

    auto size_of_descriptor(DescriptorType type) const -> uint32_t override;

    auto allocate_descriptor(DescriptorType type) -> DescriptorHandle override;
    auto allocate_descriptor(BindGroupLayout const& layout) -> DescriptorHandle override;

    // for RTV & DSV
    auto allocate_descriptor_single() -> DescriptorHandle;

    auto raw() const -> ID3D12DescriptorHeap* { return heap_.Get(); }
    auto raw_type() const -> D3D12_DESCRIPTOR_HEAP_TYPE { return type_; }

protected:
    Ref<DeviceD3D12> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    uint32_t descriptor_size_;

    DescriptorHandle start_handle_{};
    uint32_t max_count_ = 0;
    uint32_t num_used_ = 0;
};

}

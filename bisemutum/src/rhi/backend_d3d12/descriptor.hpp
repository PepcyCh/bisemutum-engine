#pragma once

#include <bisemutum/rhi/descriptor.hpp>
#include <d3d12.h>
#include <wrl.h>

namespace bi::rhi {

struct DeviceD3D12;

struct DescriptorHeapD3D12 final : DescriptorHeap {
    DescriptorHeapD3D12(Ref<DeviceD3D12> device, DescriptorHeapDesc const& desc);

    auto total_heap_size() const -> uint64_t override { return max_size_; }

    auto size_of_descriptor(DescriptorType type) const -> uint32_t override;
    auto size_of_descriptor(BindGroupLayout const& layout) const -> uint32_t override;
    auto alignment_of_descriptor(DescriptorType type) const -> uint32_t override {
        return size_of_descriptor(type);
    }
    auto alignment_of_descriptor(BindGroupLayout const& layout) const -> uint32_t override {
        return size_of_descriptor(layout);
    }

    auto start_address() const -> DescriptorHandle override { return start_handle_; }

    auto raw() const -> ID3D12DescriptorHeap* { return heap_.Get(); }
    auto raw_type() const -> D3D12_DESCRIPTOR_HEAP_TYPE { return type_; }

private:
    Ref<DeviceD3D12> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    uint32_t descriptor_size_;

    DescriptorHandle start_handle_{};
    uint64_t max_size_ = 0;
};

struct RenderTargetDescriptorHeapD3D12 final {
    RenderTargetDescriptorHeapD3D12(Ref<DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type);

    auto allocate() -> uint64_t;
    auto free(uint64_t descriptor) -> void;

private:
    Ref<DeviceD3D12> device_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    uint32_t descriptor_size_;

    struct Heap final {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
        uint64_t start;
        uint64_t curr;
        uint64_t top;
        std::vector<uint64_t> recycled;
    };
    std::vector<Heap> heaps_;
};

}

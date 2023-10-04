#include "descriptor.hpp"

#include <bisemutum/runtime/logger.hpp>

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

auto DescriptorHeapD3D12::size_of_descriptor(DescriptorType type) const -> uint32_t {
    return descriptor_size_;
}

auto DescriptorHeapD3D12::size_of_descriptor(BindGroupLayout const& layout) const -> uint32_t {
    uint32_t size = 0;
    for (auto const& entry : layout) {
        size += descriptor_size_ * entry.count;
    }
    return size;
}

RenderTargetDescriptorHeapD3D12::RenderTargetDescriptorHeapD3D12(
    Ref<DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type
)
    : device_(device), type_(type)
{
    descriptor_size_ = device->raw()->GetDescriptorHandleIncrementSize(type_);
}

auto RenderTargetDescriptorHeapD3D12::allocate() -> uint64_t {
    uint64_t addr = 0;
    for (auto& heap : heaps_) {
        if (!heap.recycled.empty()) {
            addr = heap.recycled.back();
            heap.recycled.pop_back();
        } else if (heap.curr < heap.top) {
            addr = heap.curr;
            heap.curr += descriptor_size_;
        }
        if (addr != 0) {
            break;
        }
    }
    if (addr == 0) {
        auto& heap = heaps_.emplace_back();
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{
            .Type = type_,
            .NumDescriptors = 65536,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0,
        };
        device_->raw()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap.heap));
        heap.start = heap.heap->GetCPUDescriptorHandleForHeapStart().ptr;
        heap.curr = heap.start + descriptor_size_;
        heap.top = heap.start + heap_desc.NumDescriptors * descriptor_size_;
        addr = heap.start;
    }
    return addr;
}

auto RenderTargetDescriptorHeapD3D12::free(uint64_t descriptor) -> void {
    size_t index = 0;
    uint64_t min_diff = std::numeric_limits<uint64_t>::max();
    for (size_t i = 0; i < heaps_.size(); i++) {
        auto diff = descriptor - heaps_[i].start;
        if (diff < min_diff) {
            min_diff = diff;
            index = i;
        }
    }
    BI_ASSERT(min_diff < 65536 * descriptor_size_);
    heaps_[index].recycled.push_back(descriptor);
}

}

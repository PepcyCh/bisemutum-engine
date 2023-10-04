#pragma once

#include <set>
#include <unordered_map>

#include <bisemutum/rhi/device.hpp>

namespace bi::gfx {

struct CpuDescriptorAllocator final {
    CpuDescriptorAllocator(Ref<rhi::Device> device, rhi::DescriptorHeapType type, uint32_t size_per_suballocator);

    auto allocate(rhi::DescriptorType type) -> rhi::DescriptorHandle;
    auto free(rhi::DescriptorHandle descriptor) -> void;

private:
    auto create_suballocator(uint64_t start = 0) -> void;

    Ref<rhi::Device> device_;
    rhi::DescriptorHeapType type_;
    uint32_t size_per_suballocator_;

    struct Suballocator final {
        Box<rhi::DescriptorHeap> heap;
        std::set<std::pair<uint64_t, uint64_t>> free_chunks;
    };
    std::vector<Suballocator> suballocators_;

    struct AllocationInfo final {
        size_t suballocator_index;
        size_t size;
    };
    std::unordered_map<uint64_t, AllocationInfo> allocations_;
};

struct GpuDescriptorAllocator final {
    GpuDescriptorAllocator(
        Ref<rhi::Device> device, rhi::DescriptorHeapType type,
        uint32_t total_size, uint32_t size_per_suballocator,
        uint32_t num_frames, uint32_t num_threads = 1
    );

    auto allocate(
        rhi::BindGroupLayout const& layout, uint32_t frame_index, uint32_t thread_index = 0
    ) -> rhi::DescriptorHandle;
    auto reset(uint32_t frame_index) -> void;

    auto heap() -> Ref<rhi::DescriptorHeap>;

private:
    auto create_suballocator(uint32_t frame_index, uint32_t thread_index) -> void;

    Ref<rhi::Device> device_;
    rhi::DescriptorHeapType type_;
    uint32_t total_size_;
    uint32_t size_per_suballocator_;
    uint32_t num_frames_;
    uint32_t num_threads_;

    struct Suballocator final {
        uint32_t frame_index;
        uint32_t thread_index;
        rhi::DescriptorHandle start_handle;
        uint64_t size;
        uint64_t curr;
        uint32_t chunk_id;
    };
    std::vector<std::vector<Suballocator>> suballocators_;

    Box<rhi::DescriptorHeap> heap_;
    std::vector<Box<rhi::DescriptorHeap>> suballocator_heaps_;

    uint32_t num_chunks_;
    uint32_t curr_chunk_ = 0;
    std::vector<uint32_t> recycled_chunks_;
};

}

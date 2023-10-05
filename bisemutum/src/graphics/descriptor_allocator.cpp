#include "descriptor_allocator.hpp"

#include <bisemutum/prelude/math.hpp>
#include <bisemutum/runtime/logger.hpp>

namespace bi::gfx {

CpuDescriptorAllocator::CpuDescriptorAllocator(
    Ref<rhi::Device> device, rhi::DescriptorHeapType type, uint32_t size_per_suballocator
)
    : device_(device), type_(type), size_per_suballocator_(size_per_suballocator)
{
    create_suballocator();
}

auto CpuDescriptorAllocator::allocate(rhi::DescriptorType type) -> rhi::DescriptorHandle {
    rhi::DescriptorHandle descriptor{};
    Ptr<rhi::DescriptorHeap> heap;
    auto needed_size = suballocators_[0].heap->size_of_descriptor(type);
    auto needed_alignment = suballocators_[0].heap->alignment_of_descriptor(type);
    AllocationInfo allocation{.size = needed_size};

    for (auto suballocator_index = 0; auto& suballocator : suballocators_) {
        for (auto it = suballocator.free_chunks.begin(); it != suballocator.free_chunks.end(); ++it) {
            auto [begin, end] = *it;
            auto aligned_begin = aligned_size<uint64_t>(begin, needed_alignment);
            auto allocation_end = aligned_begin + needed_size;
            if (allocation_end <= end) {
                descriptor.cpu = suballocator.heap->start_address().cpu + aligned_begin;
                if (aligned_begin != begin) {
                    suballocator.free_chunks.emplace_hint(it, begin, aligned_begin);
                }
                if (end != allocation_end) {
                    suballocator.free_chunks.emplace_hint(it, allocation_end, end);
                }
                suballocator.free_chunks.erase(it);
                heap = suballocator.heap.ref();
                allocation.suballocator_index = suballocator_index;
                break;
            }
        }
        if (descriptor.cpu != 0) {
            break;
        }
        ++suballocator_index;
    }
    if (descriptor.cpu == 0) {
        create_suballocator(needed_size);
        auto& suballocator = suballocators_.back();
        descriptor.cpu = suballocator.heap->start_address().cpu;
        heap = suballocator.heap.ref();
        allocation.suballocator_index = suballocators_.size() - 1;
    }
    heap->allocate_descriptor_at(descriptor, type);
    allocations_.insert({descriptor.cpu, allocation});
    return descriptor;
}

auto CpuDescriptorAllocator::free(rhi::DescriptorHandle descriptor) -> void {
    auto allocation_it = allocations_.find(descriptor.cpu);
    auto& suballocator = suballocators_[allocation_it->second.suballocator_index];
    suballocator.heap->free_descriptor_at(descriptor);
    auto begin = descriptor.cpu - suballocator.heap->start_address().cpu;
    auto end = begin + allocation_it->second.size;
    allocations_.erase(allocation_it);

    auto new_free_chunk = std::make_pair(begin, end);
    auto chunk_it = suballocator.free_chunks.lower_bound(new_free_chunk);
    if (chunk_it != suballocator.free_chunks.begin()) {
        auto prev = chunk_it;
        --prev;
        if (prev->second == begin) {
            new_free_chunk.first = prev->first;
            suballocator.free_chunks.erase(prev);
        }
    }
    if (chunk_it != suballocator.free_chunks.end() && chunk_it->first == end) {
        new_free_chunk.second = chunk_it->second;
        suballocator.free_chunks.erase(chunk_it);
    }
    suballocator.free_chunks.insert(new_free_chunk);
}

auto CpuDescriptorAllocator::create_suballocator(uint64_t start) -> void {
    auto& heap = suballocators_.emplace_back(device_->create_descriptor_heap(rhi::DescriptorHeapDesc{
        .max_count = size_per_suballocator_,
        .type = type_,
        .shader_visible = false,
    }));
    heap.free_chunks.emplace(start, heap.heap->total_heap_size());
}


GpuDescriptorAllocator::GpuDescriptorAllocator(
    Ref<rhi::Device> device, rhi::DescriptorHeapType type,
    uint32_t total_size, uint32_t size_per_suballocator,
    uint32_t num_frames, uint32_t num_threads
)
    : device_(device), type_(type), total_size_(total_size), size_per_suballocator_(size_per_suballocator)
    , num_frames_(num_frames), num_threads_(num_threads)
{
    suballocators_.resize(num_frames_ * num_threads_);
    total_size_ = aligned_size(total_size_, size_per_suballocator_);
    num_chunks_ = total_size_ / size_per_suballocator_;
    BI_ASSERT(num_chunks_ >= suballocators_.size());

    if (device_->properties().descriptor_heap_suballocation) {
        heap_ = device_->create_descriptor_heap(rhi::DescriptorHeapDesc{
            .max_count = total_size_,
            .type = type_,
            .shader_visible = true,
        });
        size_per_suballocator_ *= heap_->total_heap_size() / total_size_;
    } else {
        suballocator_heaps_.resize(num_chunks_);
        for (uint32_t i = 0; i < num_chunks_; i++) {
            suballocator_heaps_[i] = device_->create_descriptor_heap(rhi::DescriptorHeapDesc{
                .max_count = size_per_suballocator_,
                .type = type_,
                .shader_visible = true,
            });
        }
        size_per_suballocator_ = suballocator_heaps_[0]->total_heap_size();
    }

    for (uint32_t frame = 0; frame < num_frames_; frame++) {
        for (uint32_t thread = 0; thread < num_threads_; thread++) {
            create_suballocator(frame, thread);
        }
    }
}

auto GpuDescriptorAllocator::allocate(
    rhi::BindGroupLayout const& layout, uint32_t frame_index, uint32_t thread_index
) -> rhi::DescriptorHandle {
    auto index = frame_index * num_threads_ + thread_index;
    auto& context_suballocators = suballocators_[index];

    rhi::DescriptorHandle descriptor{};
    Ptr<rhi::DescriptorHeap> heap;
    uint32_t needed_size;
    uint32_t needed_alignment;
    if (heap_) {
        needed_size = heap_->size_of_descriptor(layout);
        needed_alignment = heap_->alignment_of_descriptor(layout);
        heap = heap_.ref();
    } else {
        auto& suballocator_heap = suballocator_heaps_[context_suballocators[0].chunk_id];
        needed_size = suballocator_heap->size_of_descriptor(layout);
        needed_alignment = suballocator_heap->alignment_of_descriptor(layout);
    }

    for (auto& suballocator : context_suballocators) {
        auto begin = aligned_size<uint64_t>(suballocator.curr, needed_alignment);
        auto end = begin + needed_size;
        if (end <= suballocator.size) {
            descriptor.cpu = suballocator.start_handle.cpu + begin;
            descriptor.gpu = suballocator.start_handle.gpu + begin;
            suballocator.curr = end;
            if (!heap) {
                heap = suballocator_heaps_[suballocator.chunk_id].ref();
            }
            break;
        }
    }
    if (descriptor.cpu == 0) {
        create_suballocator(frame_index, thread_index);
        auto& suballocator = context_suballocators.back();
        auto begin = aligned_size<uint64_t>(suballocator.curr, needed_alignment);
        auto end = begin + needed_size;
        descriptor.cpu = suballocator.start_handle.cpu + begin;
        descriptor.gpu = suballocator.start_handle.gpu + begin;
        suballocator.curr = end;
        if (!heap) {
            heap = suballocator_heaps_[suballocator.chunk_id].ref();
        }
    }

    heap->allocate_descriptor_at(descriptor, layout);
    return descriptor;
}

auto GpuDescriptorAllocator::reset(uint32_t frame_index) -> void {
    for (uint32_t i = frame_index * num_threads_; i < (frame_index + 1) * num_threads_; i++) {
        for (auto& suballocator : suballocators_[i]) {
            recycled_chunks_.push_back(suballocator.chunk_id);
            if (!heap_) {
                suballocator_heaps_[suballocator.chunk_id]->reset();
            }
            suballocator.curr = 0;
        }
    }
}

auto GpuDescriptorAllocator::heap() -> Ref<rhi::DescriptorHeap> {
    if (heap_) {
        return heap_.ref();
    } else {
        // This result will not be used, but just to keep return value not null
        return suballocator_heaps_[0].ref();
    }
}

auto GpuDescriptorAllocator::create_suballocator(uint32_t frame_index, uint32_t thread_index) -> void {
    uint32_t chunk_id = curr_chunk_;
    if (!recycled_chunks_.empty()) {
        chunk_id = recycled_chunks_.back();
        recycled_chunks_.pop_back();
    } else {
        ++curr_chunk_;
    }
    if (chunk_id >= num_chunks_) {
        log::critical("general",
            "Failed to allocate new GPU descriptor chunk of type '{}'",
            type_ == rhi::DescriptorHeapType::resource ? "resource" : "sampler"
        );
        return;
    }

    auto index = frame_index * num_threads_ + thread_index;
    auto& suballocator = suballocators_[index].emplace_back();
    suballocator.frame_index = frame_index;
    suballocator.thread_index = thread_index;
    suballocator.curr = 0;
    suballocator.size = size_per_suballocator_;
    suballocator.chunk_id = chunk_id;
    if (heap_) {
        auto offset = size_per_suballocator_ * chunk_id;
        suballocator.start_handle = heap_->start_address();
        suballocator.start_handle.cpu += offset;
        suballocator.start_handle.gpu += offset;
    } else {
        suballocator.start_handle = suballocator_heaps_[chunk_id]->start_address();
    }
}

}

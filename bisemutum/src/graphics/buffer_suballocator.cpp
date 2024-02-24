#include <bisemutum/graphics/buffer_suballocator.hpp>

#include <bisemutum/prelude/math.hpp>

namespace bi::gfx {

BufferSuballocator::BufferSuballocator(rhi::BufferDesc const& desc) : base_buffer_(desc, false) {
    free_chunks_.emplace(0, desc.size);
}

auto BufferSuballocator::reset() -> void {
    base_buffer_.reset();
    free_chunks_.clear();
}

auto BufferSuballocator::allocate(uint64_t size, uint64_t alignment) -> Option<SuballocatedBuffer> {
    for (auto it = free_chunks_.begin(); it != free_chunks_.end(); ++it) {
        auto [begin, end] = *it;
        auto aligned_begin = aligned_size<uint64_t>(begin, alignment);
        auto allocation_end = aligned_begin + size;
        if (allocation_end <= end) {
            SuballocatedBuffer allocation{};
            allocation.allocator_ = this;
            allocation.offset_ = aligned_begin;
            allocation.size_ = size;
            if (aligned_begin != begin) {
                free_chunks_.emplace_hint(it, begin, aligned_begin);
            }
            if (end != allocation_end) {
                free_chunks_.emplace_hint(it, allocation_end, end);
            }
            free_chunks_.erase(it);
            return allocation;
        }
    }
    return {};
}

auto BufferSuballocator::free(SuballocatedBuffer allocation) -> void {
    auto begin = allocation.offset_;
    auto end = begin + allocation.size_;
    auto new_free_chunk = std::make_pair(begin, end);
    auto chunk_it = free_chunks_.lower_bound(new_free_chunk);
    if (chunk_it != free_chunks_.begin()) {
        auto prev = chunk_it;
        --prev;
        if (prev->second == begin) {
            new_free_chunk.first = prev->first;
            free_chunks_.erase(prev);
        }
    }
    if (chunk_it != free_chunks_.end() && chunk_it->first == end) {
        new_free_chunk.second = chunk_it->second;
        free_chunks_.erase(chunk_it);
    }
    free_chunks_.insert(new_free_chunk);
}

}

#pragma once

#include <set>

#include "resource.hpp"

namespace bi::gfx {

struct BufferSuballocator;

struct SuballocatedBuffer final {
    auto allocator() const -> Ptr<BufferSuballocator> { return allocator_; }
    auto offset() const -> uint64_t { return offset_; }
    auto size() const -> uint64_t { return size_; }

private:
    friend BufferSuballocator;
    Ptr<BufferSuballocator> allocator_ = nullptr;
    uint64_t offset_ = 0;
    uint64_t size_ = 0;
};

struct BufferSuballocator final {
    BufferSuballocator() = default;
    BufferSuballocator(rhi::BufferDesc const& desc);

    auto has_value() const -> bool { return base_buffer_.has_value(); }
    auto reset() -> void;

    auto base_buffer() -> Buffer& { return base_buffer_; }
    auto base_buffer() const -> Buffer const& { return base_buffer_; }

    auto allocate(uint64_t size, uint64_t alignment) -> Option<SuballocatedBuffer>;
    auto free(SuballocatedBuffer allocation) -> void;

private:
    Buffer base_buffer_;
    std::set<std::pair<uint64_t, uint64_t>> free_chunks_;
};

}

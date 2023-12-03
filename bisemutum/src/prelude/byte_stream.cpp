#include <bisemutum/prelude/byte_stream.hpp>

namespace bi {

auto ReadByteStream::set_offset(size_t offset) -> void {
    curr_offset_ = std::min(offset, data_.size());
}

auto ReadByteStream::read_raw(std::byte* dst, size_t size) -> ReadByteStream& {
    if (curr_offset_ + size <= data_.size()) {
        std::copy_n(data_.data() + curr_offset_, size, dst);
        curr_offset_ += size;
    }
    return *this;
}

auto ReadByteStream::read(std::string& str) -> ReadByteStream& {
    uint64_t length = 0;
    read(length);
    str.resize(length, '\0');
    read_raw(reinterpret_cast<std::byte*>(str.data()), length);
    return *this;
}


auto WriteByteStream::reserve(size_t size) -> void {
    data_.reserve(size);
}

auto WriteByteStream::clear() -> void {
    data_.clear();
    curr_offset_ = 0;
}

auto WriteByteStream::set_offset(size_t offset) -> void {
    curr_offset_ = offset;
    if (curr_offset_ > data_.size()) {
        data_.resize(curr_offset_, {});
    }
}

auto WriteByteStream::write_raw(std::byte const* src, size_t size) -> WriteByteStream& {
    std::copy_n(src, size, std::inserter(data_, data_.begin() + curr_offset_));
    curr_offset_ += size;
    return *this;
}

auto WriteByteStream::write(std::string_view value) -> WriteByteStream& {
    write(static_cast<uint64_t>(value.size()));
    write_raw(reinterpret_cast<std::byte const*>(value.data()), value.size());
    return *this;
}

}

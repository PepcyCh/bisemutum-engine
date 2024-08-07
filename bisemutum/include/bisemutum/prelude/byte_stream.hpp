#pragma once

#include <cstddef>
#include <vector>
#include <string>

#include "span.hpp"

namespace bi {

template <typename T>
inline constexpr bool can_be_used_for_byte_stream = std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>;

struct ReadByteStream final {
    ReadByteStream() = default;
    ReadByteStream(CSpan<std::byte> data) : data_(data) {}
    ReadByteStream(std::vector<std::byte> data) : owned_data_(std::move(data)), data_(owned_data_) {}

    auto size() const -> size_t { return data_.size(); }

    auto curr_offset() const -> size_t { return curr_offset_; }
    auto set_offset(size_t offset) -> void;

    auto read_raw(std::byte* dst, size_t size) -> ReadByteStream&;

    template <typename T> requires can_be_used_for_byte_stream<T>
    auto read(T& value) -> ReadByteStream& {
        return read_raw(reinterpret_cast<std::byte*>(&value), sizeof(T));
    }

    auto read(std::string& str) -> ReadByteStream&;

    template <typename T>
    auto read(std::vector<T>& vector) -> ReadByteStream& {
        uint64_t length = 0;
        read(length);
        vector.resize(length);
        if constexpr (can_be_used_for_byte_stream<T>) {
            read_raw(reinterpret_cast<std::byte*>(vector.data()), length * sizeof(T));
        } else {
            for (uint64_t i = 0; i < length; i++) { read(vector[i]); }
        }
        return *this;
    }

    auto read_compressed_part(ReadByteStream& uncompressed_bs) -> ReadByteStream&;

private:
    std::vector<std::byte> owned_data_;
    CSpan<std::byte> data_;
    size_t curr_offset_ = 0;
};

struct WriteByteStream final {
    auto data() const -> CSpan<std::byte> { return data_; }

    auto reserve(size_t size) -> void;
    auto clear() -> void;

    auto curr_offset() const -> size_t { return curr_offset_; }
    auto set_offset(size_t offset) -> void;

    auto write_raw(std::byte const* src, size_t size) -> WriteByteStream&;

    template <typename T> requires can_be_used_for_byte_stream<T>
    auto write(T const& value) -> WriteByteStream& {
        return write_raw(reinterpret_cast<std::byte const*>(&value), sizeof(T));
    }

    auto write(std::string_view str) -> WriteByteStream&;
    auto write(std::string const& str) -> WriteByteStream& { return write(std::string_view{str}); }
    auto write(char const* str) -> WriteByteStream& { return write(std::string_view{str}); }

    template <typename T>
    auto write(std::vector<T> const& vector) -> WriteByteStream& {
        write(static_cast<uint64_t>(vector.size()));
        if constexpr (can_be_used_for_byte_stream<T>) {
            write_raw(reinterpret_cast<std::byte const*>(vector.data()), vector.size() * sizeof(T));
        } else {
            for (auto const& elem : vector) { write(elem); }
        }
        return *this;
    }

    auto compress_data(size_t from = 0) -> WriteByteStream&;

private:
    std::vector<std::byte> data_;
    size_t curr_offset_ = 0;
};

}

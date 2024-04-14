#include <bisemutum/prelude/byte_stream.hpp>

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

namespace bi {

auto ReadByteStream::set_offset(size_t offset) -> void {
    curr_offset_ = std::min(offset, data_.size());
}

auto ReadByteStream::read_raw(std::byte* dst, size_t size) -> ReadByteStream& {
    if (curr_offset_ + size <= data_.size() && size > 0) {
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

auto ReadByteStream::read_compressed_part(ReadByteStream& uncompressed_bs) -> ReadByteStream& {
    uint64_t data_length = 0;
    uint64_t compressed_length = 0;
    read(data_length).read(compressed_length);

    std::vector<std::byte> uncompressed_data{data_length};
    mz_ulong temp_data_length = data_length;
    mz_uncompress(
        reinterpret_cast<unsigned char*>(uncompressed_data.data()), &temp_data_length,
        reinterpret_cast<unsigned char const*>(data_.data() + curr_offset_), compressed_length
    );
    curr_offset_ += compressed_length;

    uncompressed_bs = ReadByteStream{std::move(uncompressed_data)};

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
    if (size > 0) {
        std::copy_n(src, size, std::inserter(data_, data_.begin() + curr_offset_));
        curr_offset_ += size;
    }
    return *this;
}

auto WriteByteStream::write(std::string_view value) -> WriteByteStream& {
    write(static_cast<uint64_t>(value.size()));
    write_raw(reinterpret_cast<std::byte const*>(value.data()), value.size());
    return *this;
}

auto WriteByteStream::compress_data(size_t from) -> WriteByteStream& {
    auto data_length = curr_offset_ - from;
    auto compressed_length = mz_compressBound(data_length);
    std::vector<std::byte> compressed_data{compressed_length};
    mz_compress(
        reinterpret_cast<unsigned char*>(compressed_data.data()), &compressed_length,
        reinterpret_cast<unsigned char const*>(data_.data() + from), data_length
    );
    compressed_data.resize(compressed_length);

    std::vector<std::byte> last_data{data_.begin() + curr_offset_, data_.end()};
    data_.resize(from);
    curr_offset_ = from;
    write(static_cast<uint64_t>(data_length));
    write(compressed_data);
    auto temp_offset = curr_offset_;
    write_raw(last_data.data(), last_data.size());
    curr_offset_ = temp_offset;

    return *this;
}

}

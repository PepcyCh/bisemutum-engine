#pragma once

#include <string>

#include "../prelude/span.hpp"

namespace bi::crypto {

struct MD5 final {
    static constexpr size_t size = 128 / 8;
    auto as_string() const -> std::string;
    std::byte data[size];
};
auto md5(CSpan<std::byte> in_data) -> MD5;

struct SHA256 final {
    static constexpr size_t size = 256 / 8;
    auto as_string() const -> std::string;
    std::byte data[size];
};
auto sha256(CSpan<std::byte> in_data) -> SHA256;

}

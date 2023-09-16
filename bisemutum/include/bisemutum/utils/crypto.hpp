#pragma once

#include <string>

#include "../prelude/span.hpp"

namespace bi::crypto {

struct MD5 final {
    static constexpr size_t size = 128 / 8;
    auto as_string() const -> std::string;
    auto operator==(MD5 const& rhs) const noexcept -> bool = default;
    std::byte data[size];
};
auto md5(CSpan<std::byte> in_data) -> MD5;

struct SHA256 final {
    static constexpr size_t size = 256 / 8;
    auto as_string() const -> std::string;
    auto operator==(SHA256 const& rhs) const noexcept -> bool = default;
    std::byte data[size];
};
auto sha256(CSpan<std::byte> in_data) -> SHA256;

}

template <>
struct std::hash<bi::crypto::MD5> final {
    auto operator()(bi::crypto::MD5 const& v) const noexcept -> size_t;
};

template <>
struct std::hash<bi::crypto::SHA256> final {
    auto operator()(bi::crypto::SHA256 const& v) const noexcept -> size_t;
};

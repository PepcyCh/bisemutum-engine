#pragma once

#include <utility>
#include <string>

#include "traits.hpp"

namespace bi {

inline auto hash_combine(size_t seed, size_t v) -> size_t {
    seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}
template <typename T>
auto hash_combine(size_t seed, T const& v) -> size_t {
    std::hash<T> hasher;
    return hash_combine(seed, hasher(v));
}

template <typename T>
auto hash(T const& v) -> size_t {
    std::hash<T> hasher;
    return hasher(v);
}

template <typename T, typename... Args>
auto hash(T const& first, Args &&... last) -> size_t {
    size_t result = hash(last...);
    return hash_combine(result, first);
}

template <typename T>
struct ByteHash final {
    auto operator()(T const& v) const noexcept -> size_t {
        auto p = reinterpret_cast<uint8_t const*>(&v);
        size_t hash = 0;
        for (size_t i = 0; i < sizeof(T); i++) {
            hash = hash_combine(hash, p[i]);
        }
        return hash;
    }
};

struct StringHash final {
    using is_transparent = void;

    size_t operator()(char const* str) const { return std::hash<std::string_view>{}(str); }
    size_t operator()(std::string_view str) const { return std::hash<std::string_view>{}(str); }
    size_t operator()(std::string const& str) const { return std::hash<std::string_view>{}(str); }
};

}

template<typename T1, typename T2>
requires bi::traits::Hashable<T1, std::hash<T1>> && bi::traits::Hashable<T2, std::hash<T2>>
struct std::hash<std::pair<T1, T2>> {
    auto operator()(std::pair<T1, T2> const& v) const noexcept -> size_t {
        return bi::hash(v.first, v.second);
    }
};

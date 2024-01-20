#pragma once

#include <utility>
#include <string>

#include "traits.hpp"

namespace bi {

template <typename T>
auto hash_combine(size_t seed, T const& v) -> size_t {
    auto hashed_v = std::hash<T>{}(v);
    seed ^= hashed_v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

template <typename T>
auto hash(T const& v) -> size_t {
    return std::hash<T>{}(v);
}

namespace details {

template <typename T, size_t curr = 0>
auto hash_helper(size_t& curr_hash, T const& first) -> size_t {
    curr_hash = hash_combine(curr_hash, first);
    return curr_hash;
}

template <typename T, typename... Last, size_t curr = 0>
auto hash_helper(size_t& curr_hash, T const& first, Last&&... last) -> size_t {
    curr_hash = hash_combine(curr_hash, first);
    return hash_helper(curr_hash, last...);
}

} // namespace details

template <typename T, typename... Last>
auto hash(T const& first, Last&&... last) -> size_t {
    size_t curr_hash = hash(first);
    return details::hash_helper(curr_hash, last...);
}

template <typename T>
struct ByteHash final {
    auto operator()(T const& v) const noexcept -> size_t {
        auto p = reinterpret_cast<std::byte const*>(&v);
        size_t hash = 0;
        for (size_t i = 0; i < sizeof(T); i++) {
            hash = hash_combine(hash, p[i]);
        }
        return hash;
    }
};
template <typename T>
auto hash_by_byte(T const& v) -> size_t {
    return ByteHash<T>{}(v);
}

template <typename T>
struct LinearContainerHash final {
    auto operator()(T const& v) const noexcept -> size_t {
        size_t hash = v.size();
        for (auto const& elem : v) {
            hash = hash_combine(hash, elem);
        }
        return hash;
    }
};
template <typename T>
auto hash_linear_container(T const& v) -> size_t {
    return LinearContainerHash<T>{}(v);
}

struct StringHash final {
    using is_transparent = void;

    size_t operator()(char const* str) const { return std::hash<std::string_view>{}(str); }
    size_t operator()(std::string_view str) const { return std::hash<std::string_view>{}(str); }
    size_t operator()(std::string const& str) const { return std::hash<std::string_view>{}(str); }
};

namespace details {

template <size_t curr, typename... Ts>
auto hash_tuple_helper(size_t& curr_hash, std::tuple<Ts...> const& v) -> size_t {
    if constexpr (curr == sizeof...(Ts)) {
        return curr_hash;
    } else if constexpr (curr == 0) {
        curr_hash = hash(std::get<0>(v));
        return hash_tuple_helper<curr + 1, Ts...>(curr_hash, v);
    } else {
        curr_hash = hash_combine(curr_hash, std::get<curr>(v));
        return hash_tuple_helper<curr + 1, Ts...>(curr_hash, v);
    }
}

} // namespace details

} // namespace bi

template<typename T1, typename T2>
requires bi::traits::Hashable<T1, std::hash<T1>> && bi::traits::Hashable<T2, std::hash<T2>>
struct std::hash<std::pair<T1, T2>> {
    auto operator()(std::pair<T1, T2> const& v) const noexcept -> size_t {
        return bi::hash(v.first, v.second);
    }
};

template<typename... Ts>
struct std::hash<std::tuple<Ts...>> {
    auto operator()(std::tuple<Ts...> const& v) const noexcept -> size_t {
        size_t curr_hash = 0;
        return bi::details::hash_tuple_helper<0>(curr_hash, v);
    }
};

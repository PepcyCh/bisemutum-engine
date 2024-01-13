#pragma once

#include <type_traits>
#include <algorithm>
#include <vector>

namespace bi {

template <typename... Ts>
struct TypeList final {};

template <typename T, typename... Ts>
constexpr auto type_push_back(TypeList<Ts...>) -> TypeList<Ts..., T>;

template <typename T, typename... Ts>
constexpr auto type_push_front(TypeList<Ts...>) -> TypeList<T, Ts...>;

template <size_t N>
struct ConstexprStringLit final {
    constexpr ConstexprStringLit(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }
    char value[N];
};

namespace details {

template <typename T, std::integral ElemenetT = size_t, size_t curr_index = 0>
constexpr auto get_array_sizes_helper(std::vector<ElemenetT>& array_sizes) -> void {
    if constexpr (curr_index < std::rank_v<T>) {
        array_sizes.push_back(static_cast<ElemenetT>(std::extent_v<T, curr_index>));
        get_array_sizes_helper<T, ElemenetT, curr_index + 1>(array_sizes);
    }
}

}

template <typename T, std::integral ElemenetT = size_t>
constexpr auto get_array_sizes() -> std::vector<ElemenetT> {
    std::vector<ElemenetT> array_sizes{};
    array_sizes.reserve(std::rank_v<T>);
    details::get_array_sizes_helper<T>(array_sizes);
    return array_sizes;
}

}

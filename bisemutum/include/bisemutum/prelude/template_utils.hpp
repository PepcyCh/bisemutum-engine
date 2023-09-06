#pragma once

#include <algorithm>

namespace bi {

template <typename... Ts>
struct TypeList final {};

template <typename T, typename... Ts>
constexpr auto type_push_back(TypeList<Ts...>) -> TypeList<Ts..., T>;

template <typename T, typename... Ts>
constexpr auto type_push_front(TypeList<Ts...>) -> TypeList<T, Ts...>;

template<size_t N>
struct ConstexprStringLit final {
    constexpr ConstexprStringLit(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }
    char value[N];
};

}

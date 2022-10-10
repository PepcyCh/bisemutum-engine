#pragma once

#include <type_traits>
#include <concepts>

#include "bismuth.hpp"

BISMUTH_NAMESPACE_BEGIN

template <typename T>
concept EnumT = std::is_enum_v<T>;

template <typename T>
concept ArrayT = std::is_array_v<T>;
template <typename T>
concept NonArrayT = !std::is_array_v<T>;

template <typename T, typename H>
concept Hashable = requires (T v1, T v2) {
    { H{}(v1) } -> std::convertible_to<size_t>;
    { v1 == v2 } -> std::convertible_to<bool>;
};

BISMUTH_NAMESPACE_END

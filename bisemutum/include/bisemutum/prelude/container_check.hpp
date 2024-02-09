#pragma once

#include <vector>

namespace bi::traits {

template <typename T>
struct IsStdVector final : std::false_type {};
template <typename T>
struct IsStdVector<std::vector<T>> final : std::true_type {};
template <typename T>
constexpr bool is_std_vector_v = IsStdVector<T>::value;

}

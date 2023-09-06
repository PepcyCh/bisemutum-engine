#pragma once

#include <type_traits>
#include <concepts>
#include <compare>

namespace bi::traits {

template <typename T>
constexpr bool AlwaysFalse = false;

template <typename T>
concept Void = std::is_void_v<T>;

template <typename T>
struct Nonzeroable final {
    static constexpr bool value = false;
};
template <typename T>
constexpr bool nonzeroable_v = Nonzeroable<T>::value;

template <typename T>
concept Enum = std::is_enum_v<T>;
template <typename T>
concept Array = std::is_array_v<T>;
template <typename T>
concept NonArray = !std::is_array_v<T>;

template <typename T, typename H>
concept Hashable = requires (T v1, T v2) {
    { H{}(v1) } -> std::convertible_to<size_t>;
    { v1 == v2 } -> std::convertible_to<bool>;
};

template <typename T>
concept Sized = requires { sizeof(T); };

template <typename T>
concept Range = requires (T v) {
    std::is_same_v<decltype(v.begin()), decltype(v.end())>;
};

template <typename T, typename... Ts>
struct OneOfHelper;
template <typename T, typename First, typename... Last>
struct OneOfHelper<T, First, Last...> final {
    static constexpr bool value = std::is_same_v<T, First> || OneOfHelper<T, Last...>::value;
};
template <typename T>
struct OneOfHelper<T> final {
    static constexpr bool value = false;
};
template <typename T, typename... Ts>
constexpr bool one_of_v = OneOfHelper<T, Ts...>::value;
template <typename T, typename... Ts>
concept OneOf = one_of_v<T, Ts...>;

template <typename From, typename To>
concept StaticCastConvertibleTo = requires { static_cast<To>(std::declval<From>()); };
template <typename To, typename From>
concept StaticCastConvertibleFrom = requires { static_cast<To>(std::declval<From>()); };

}

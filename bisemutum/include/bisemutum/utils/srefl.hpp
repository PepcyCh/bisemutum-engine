#pragma once

#include <string_view>
#include <tuple>
#include <limits>

#include <magic_enum.hpp>

#include "../macros/function.hpp"
#include "../macros/utils.hpp"

// Some basic attributes
namespace bi {

struct RangeF final {
    float min = 0.0f;
    float max = std::numeric_limits<float>::max();
    float step = 0.01f;
};
struct RangeI final {
    int min = 0;
    int max = std::numeric_limits<int>::max();
    int step = 1;
};

// Mark a `float3` variable that represents color.
struct Color final {};
// Mark a `float3` variable that represents Euler angle.
struct Rotation final {};

}

namespace bi::srefl {

namespace details {

template <size_t I, typename AttribT, typename... AttribTs>
constexpr auto has_attribute_helper(std::tuple<AttribTs...> const& attributes) -> bool {
    if constexpr (I < sizeof...(AttribTs)) {
        if constexpr (std::is_same_v<std::tuple_element_t<I, std::tuple<AttribTs...>>, AttribT>) {
            return true;
        } else {
            return has_attribute_helper<I + 1, AttribT, AttribTs...>(attributes);
        }
    } else {
        return false;
    }
}

template <size_t I, typename AttribT, typename... AttribTs>
constexpr auto get_attribute_helper(
    std::tuple<AttribTs...> const& attributes, AttribT const& fallback
) -> AttribT const& {
    if constexpr (I < sizeof...(AttribTs)) {
        if constexpr (std::is_same_v<std::tuple_element_t<I, std::tuple<AttribTs...>>, AttribT>) {
            return std::get<I>(attributes);
        } else {
            return get_attribute_from<I + 1, AttribT, AttribTs...>(attributes);
        }
    } else {
        return fallback;
    }
}

} // namespace details

template <typename TypeT, typename FieldT, typename... AttribTs>
struct FieldInfo final {
    using FieldType = FieldT;
    using TypeType = TypeT;

    std::string_view name;
    FieldT TypeT::* ptr;
    std::tuple<AttribTs...> attributes;
    static constexpr bool is_const = std::is_const_v<FieldT>;

    consteval FieldInfo(
        std::string_view name, FieldT TypeT::* ptr, AttribTs... attribs
    ) : name(name), ptr(ptr), attributes(std::make_tuple(attribs...)) {}

    constexpr auto operator()(TypeT& o) -> FieldT& { return o.*ptr; }
    constexpr auto operator()(TypeT const& o) const -> FieldT const& { return o.*ptr; }

    template <typename AttribT>
    constexpr auto has_attribute() const -> bool {
        return details::has_attribute_helper<0, AttribT, AttribTs...>(attributes);
    }

    template <typename AttribT>
    constexpr auto get_attribute(AttribT const& fallback = AttribT{}) const -> AttribT const& {
        return details::get_attribute_helper<0, AttribT, AttribTs...>(attributes, fallback);
    }
};

#define BI_SREFL_CURRENT_NAMESPACE \
    ([]() { \
        constexpr std::string_view func_name = BI_FUNCTION_SIG; \
        constexpr size_t start = func_name.find_first_of("<") + 1; \
        constexpr size_t end = func_name.find_first_of(",>"); \
        constexpr auto s = func_name.substr(start, end - start); \
        constexpr size_t ns_end = s.rfind("::"); \
        return s.substr(0, ns_end == std::string_view::npos ? 0 : ns_end); \
    }())

template <typename TypeT, typename... FieldTs>
struct TypeInfo final {
    using value_type = TypeT;
    std::string_view namespace_name = BI_SREFL_CURRENT_NAMESPACE;
    std::string_view type_name;
    std::tuple<FieldTs...> members;
};

template <typename TypeT, typename... FieldTs, typename FieldT>
consteval auto operator+(TypeInfo<TypeT, FieldTs...> lhs, FieldT rhs) -> TypeInfo<TypeT, FieldTs..., FieldT> {
    return TypeInfo<TypeT, FieldTs..., FieldT>{
        .namespace_name = lhs.namespace_name,
        .type_name = lhs.type_name,
        .members = std::tuple_cat(lhs.members, std::make_tuple(rhs)),
    };
}

template <typename T>
consteval auto refl_ptr(T const*) -> TypeInfo<T> {
    return TypeInfo<T>{
        .type_name = {"<unknown type>"},
    };
}
template <typename T>
consteval auto refl() -> decltype(auto) {
    T const* ptr = nullptr;
    return refl_ptr(ptr);
}

template <typename T>
constexpr bool is_reflectable = refl<T>().type_name != "<unknown type>";

template <size_t I = 0, typename Func, typename... Ts>
constexpr auto for_each(std::tuple<Ts...> const& members, Func func) -> void {
    if constexpr (I < sizeof...(Ts)) {
        func(std::get<I>(members));
        for_each<I + 1, Func, Ts...>(members, func);
    }
}

#define BI_SREFL_type(ty, ...) consteval auto refl_ptr(ty const*) -> decltype(auto) { \
    return ::bi::srefl::TypeInfo<ty>{.type_name = #ty}
#define BI_SREFL_ID_type(ty, ...) ty
#define BI_SREFL(ty, ...) BI_EVAL_MACRO_ONCE(BI_SREFL_##ty) \
    __VA_OPT__(BI_SREFL_INNER(BI_EVAL_MACRO_ONCE(BI_SREFL_ID_##ty), __VA_ARGS__)) \
    ; }

#define BI_SREFL_INNER(ty, ...) __VA_OPT__(BI_EVAL_MACRO(BI_SREFL_INNER_HELPER(ty, __VA_ARGS__)))
#define BI_SREFL_INNER_HELPER(ty, first, ...) BI_EVAL_MACRO_ONCE( \
        BI_EVAL_MACRO_ONCE(BI_SREFL_NAME_##first)BI_EXPAND_ARGS(ty, BI_EVAL_MACRO_ONCE(BI_SREFL_LIST_##first)) \
    ) __VA_OPT__(BI_SREFL_INNER_HELPER2 BI_PARENS (ty, __VA_ARGS__))
#define BI_SREFL_INNER_HELPER2() BI_SREFL_INNER_HELPER

#define BI_SREFL_LIST_field(...) __VA_ARGS__
#define BI_SREFL_NAME_field(...) BI_SREFL_field
#define BI_SREFL_field(ty, nm, ...) + ::bi::srefl::FieldInfo(#nm, &ty::nm __VA_OPT__(,) __VA_ARGS__)

}

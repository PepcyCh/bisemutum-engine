#pragma once

#include <string>
#include <tuple>
#include <utility>

#include <magic_enum.hpp>

#include "../macros/function.hpp"
#include "../macros/utils.hpp"

namespace bi::srefl {

#define BI_SREFL_CURRENT_NAMESPACE \
    ([]() { \
        constexpr std::string_view func_name = BI_FUNCTION_SIG; \
        constexpr size_t start = func_name.find_first_of("<") + 1; \
        constexpr size_t end = func_name.find_first_of(",>"); \
        constexpr auto s = func_name.substr(start, end - start); \
        constexpr size_t ns_end = s.rfind("::"); \
        return s.substr(0, ns_end == std::string_view::npos ? 0 : ns_end); \
    }())

template <typename TypeT, typename FieldT>
struct FieldInfo final {
    using FieldType = FieldT;
    using TypeType = TypeT;

    std::string_view name;
    FieldT TypeT::* ptr;
    static constexpr bool is_const = std::is_const_v<FieldT>;

    consteval FieldInfo(std::string_view name, FieldT TypeT::* ptr) : name(name), ptr(ptr) {}

    constexpr auto operator()(TypeT& o) -> FieldT& { return o.*ptr; }
    constexpr auto operator()(TypeT const& o) const -> FieldT const& { return o.*ptr; }
    constexpr auto get(TypeT& o) -> FieldT& { return o.*ptr; }
    constexpr auto get(TypeT const& o) const -> FieldT const& { return o.*ptr; }
};

template <typename TypeT, typename... FieldTs>
struct TypeInfo final {
    using value_type = TypeT;
    std::string_view namespace_name = BI_SREFL_CURRENT_NAMESPACE;
    std::string_view type_name;
    std::tuple<FieldInfo<TypeT, FieldTs>...> members;
};

template <typename TypeT, typename... FieldTs, typename FieldT>
consteval auto operator+(TypeInfo<TypeT, FieldTs...> lhs, FieldInfo<TypeT, FieldT> rhs) -> TypeInfo<TypeT, FieldTs..., FieldT> {
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
#define BI_SREFL_field(ty, nm, ...) + ::bi::srefl::FieldInfo(#nm, &ty::nm)

}

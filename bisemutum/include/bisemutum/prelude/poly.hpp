#pragma once

#include <anyany/anyany.hpp>
#include <anyany/anyany_macro.hpp>

#include "template_utils.hpp"
#include "../macros/map.hpp"

namespace bi {

template <typename T>
struct Dyn;
template <typename... Fs>
struct Dyn<TypeList<Fs...>> final {
    using Box = aa::any_with<Fs...>;
    using Ref = aa::ref<Fs...>;
    using CRef = aa::cref<Fs...>;
    using Ptr = aa::ptr<Fs...>;
    using CPtr = aa::cptr<Fs...>;

    Dyn() = delete;
};

#define BI_TRAIT_BEGIN(name, ...) struct name##_functions final { \
    name##_functions() = delete; \
    template <size_t index> struct FuncsList { using type = FuncsList<index - 1>::type; }; \
    template <> struct FuncsList<__LINE__> { using type = ::bi::TypeList<BI_MACRO_MAP_OP_LIST(BI_MACRO_MAP_OP_SCOPE, aa, __VA_ARGS__)>; };
#define BI_TRAIT_METHOD(name, func) anyany_method(name, func); \
    template <> struct FuncsList<__LINE__> { using type = decltype(::bi::type_push_back<name>(FuncsList<__LINE__ - 1>::type{})); };
#define BI_TRAIT_END(name) }; using name = name##_functions::FuncsList<__LINE__ - 1>::type;


template <typename T>
auto is_poly_ptr_address_same(typename Dyn<T>::Ptr a, typename Dyn<T>::Ptr b) -> bool {
    return a.raw() == b.raw();
}
template <typename T>
auto is_poly_ptr_address_same(typename Dyn<T>::CPtr a, typename Dyn<T>::CPtr b) -> bool {
    return a.raw() == b.raw();
}

}

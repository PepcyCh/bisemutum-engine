#pragma once

#include <cassert>

#include "option.hpp"

namespace bi {

template <typename T>
struct Ref final {
    template <typename T2> requires std::convertible_to<T2*, T*>
    Ref(Ref<T2> rhs) noexcept : ptr_(rhs.ptr_) {}

    auto operator=(Ref<T> rhs) noexcept -> Ref& { ptr_ = rhs.ptr_; return *this; }
    template <typename T2> requires std::convertible_to<T2*, T*>
    auto operator=(Ref<T2> rhs) noexcept -> Ref& { ptr_ = rhs.ptr_; return *this; }

    Ref(T& object) noexcept : ptr_(&object) {}
    template <typename T2> requires std::convertible_to<T2*, T*>
    Ref(T2& object) noexcept : ptr_(&object) {}

    static auto unsafe_make(T* ptr) noexcept -> Ref {
        assert(ptr && "initialize Ref from null pointer");
        return Ref(ptr);
    }

    auto operator==(Ref const& rhs) const -> bool = default;
    auto operator<=>(Ref const& rhs) const = default;

    auto operator*() const noexcept(noexcept(*std::declval<T*>())) -> std::add_lvalue_reference_t<T> {
        return *ptr_;
    }
    auto operator->() const noexcept -> T* { return ptr_; }
    auto get() const noexcept -> T* { return ptr_; }

    template <typename T2> requires requires { static_cast<T2*>(std::declval<T*>()); }
    auto cast_to() const -> Ref<T2> {
        return Ref<T2>(static_cast<T2*>(ptr_));
    }

    template <typename T2> requires requires { dynamic_cast<T2*>(std::declval<T*>()); }
    auto dyn_cast_to() const -> bi::Option<Ref<T2>> {
        T2 *ptr2 = dynamic_cast<T2*>(ptr_);
        return ptr2 ? Ref<T2>(ptr2) : bi::Option<Ref<T2>>{};
    }

    auto remove_const() const -> Ref<std::remove_const_t<T>> {
        return Ref<std::remove_const_t<T>>(const_cast<std::remove_const_t<T>*>(ptr_));
    }

private:
    Ref(T* ptr) noexcept : ptr_(ptr) {}

    template <typename T2>
    friend class Ref;

    T* ptr_ = nullptr;
};
template <typename T>
struct traits::Nonzeroable<Ref<T>> final {
    static constexpr bool value = true;
};

template <typename T>
using CRef = Ref<std::add_const_t<T>>;

template <typename T>
auto make_ref(T& object) -> Ref<T> {
    return Ref<T>(object);
}
template <typename T>
auto make_cref(T const& object) -> CRef<T> {
    return CRef<T>(object);
}

template <typename T>
auto unsafe_make_ref(T* ptr) -> Ref<T> {
    return Ref<T>::unsafe_make(ptr);
}
template <typename T>
auto unsafe_make_cref(T* ptr) -> CRef<T> {
    return CRef<T>::unsafe_make(ptr);
}

template <typename T>
struct Option<Ref<T>> final {
    Option() = default;

    Option(T* ptr) : ptr_(ptr) {}
    template <typename T2> requires std::convertible_to<T2*, T*>
    Option(T2* ptr) noexcept : ptr_(ptr) {}

    template <typename T2> requires std::convertible_to<T2*, T*>
    Option(Option<Ref<T2>> ref) noexcept : ptr_(ref.get()) {}

    Option(Ref<T> ref) : ptr_(ref.get()) {}
    template <typename T2> requires std::convertible_to<T2*, T*>
    Option(Ref<T2> ref) noexcept : ptr_(ref.get()) {}

    auto operator=(Ref<T> rhs) noexcept -> Option& { ptr_ = rhs.get(); return *this; }
    template <typename T2> requires std::convertible_to<T2*, T*>
    auto operator=(Ref<T2> rhs) noexcept -> Option& { ptr_ = rhs.get(); return *this; }
    auto operator=(T* ptr) noexcept -> Option& { ptr_ = ptr; return *this; }
    auto operator=(std::nullptr_t) noexcept -> Option& { ptr_ = nullptr; return *this; }

    auto operator==(Option const& rhs) const -> bool = default;
    auto operator<=>(Option const& rhs) const = default;

    auto operator*() const noexcept(noexcept(*std::declval<T*>())) -> std::add_lvalue_reference_t<T> {
        return *ptr_;
    }
    auto operator->() const noexcept -> T* { return ptr_; }
    auto get() const noexcept -> T* { return ptr_; }

    template <typename T2> requires requires { static_cast<T2*>(std::declval<T*>()); }
    auto cast_to() const -> Option<Ref<T2>> {
        return ptr_ ? Ref<T2>(static_cast<T2*>(ptr_)) : Option{};
    }

    template <typename T2> requires requires { dynamic_cast<T2*>(std::declval<T*>()); }
    auto dyn_cast_to() const -> Option<Ref<T2>> {
        T2 *ptr2 = ptr_ ? dynamic_cast<T2*>(ptr_) : nullptr;
        return ptr2 ? Ref<T2>(ptr2) : Option<Ref<T2>>{};
    }

    auto remove_const() const -> Option<Ref<std::remove_const_t<T>>> {
        return ptr_ ? Ref<std::remove_const_t<T>>(const_cast<std::remove_const_t<T>*>(ptr_)) : Option{};
    }

    auto value() -> Ref<T> { return unsafe_make_ref(ptr_); }
    auto value() const -> CRef<T> { return unsafe_make_cref(ptr_); }

    auto value_or(CRef<T> fallback) const -> CRef<T> {
        return has_value() ? unsafe_make_cref(ptr_) : fallback;
    }

    auto has_value() const -> bool { return ptr_ != nullptr; }
    operator bool() const { return has_value(); }

    auto reset() -> void { ptr_ = nullptr; }

private:
    T* ptr_ = nullptr;
};

template <typename T>
using Ptr = Option<Ref<T>>;

template <typename T>
using CPtr = Option<CRef<T>>;

}

template <typename T>
struct std::hash<bi::Ref<T>> final {
    auto operator()(bi::Ref<T> v) const noexcept -> size_t {
        return std::hash<T*>{}(v.get());
    }
};

#pragma once

#include <cassert>

#include "traits.hpp"

namespace bi {

namespace detail {

template <typename T>
struct RcInner final {
    size_t ref_count = 0;
    T* ptr = nullptr;

    RcInner(T* ptr) noexcept : ref_count(1), ptr(ptr) {}

    auto increase_ref_count() -> void { ++ref_count; }

    auto decrease_ref_count() -> void {
        --ref_count;
        if (ref_count == 0) {
            delete ptr;
        }
    }
};

}

template <typename T>
struct Rc final {
    Rc() = default;

    template <typename... Args> requires std::constructible_from<T, Args...>
    static auto make(Args&&... args) noexcept -> Rc {
        return Rc(new T(std::forward<Args>(args)...));
    }

    static auto unsafe_make(T* ptr) noexcept -> Rc {
        assert(ptr && "initialize Rc from null pointer");
        return Rc(ptr);
    }

    ~Rc() {
        if (inner_) {
            inner_->decrease_ref_count();
            if (inner_->ref_count == 0) {
                delete inner_;
                inner_ = nullptr;
            }
        }
    }

    Rc(Rc const& rhs) noexcept : inner_(rhs.inner_) {
        if (inner_) {
            inner_->increase_ref_count();
        }
    }
    Rc(Rc&& rhs) noexcept : inner_(rhs.inner_) { rhs.inner_ = nullptr; }

    template <typename T2> requires std::convertible_to<T2*, T*>
    Rc(Rc<T2> const& rhs) noexcept : inner_(reinterpret_cast<detail::RcInner<T>*>(rhs.inner_)) {
        if (inner_) {
            inner_->increase_ref_count();
        }
    }
    template <typename T2> requires std::convertible_to<T2*, T*>
    Rc(Rc<T2>&& rhs) noexcept : inner_(reinterpret_cast<detail::RcInner<T>*>(rhs.inner_)) { rhs.inner_ = nullptr; }

    auto operator=(Rc const& rhs) noexcept -> Rc& {
        if (this != std::addressof(rhs)) {
            ~Rc();
            inner_ = rhs.inner_;
            if (inner_) {
                inner_->increase_ref_count();
            }
        }
        return *this;
    }
    auto operator=(Rc&& rhs) noexcept -> Rc& {
        if (this != std::addressof(rhs)) {
            ~Rc();
            inner_ = rhs.inner_;
            rhs.inner_ = nullptr;
        }
        return *this;
    }

    template <typename T2> requires std::convertible_to<T2*, T*>
    auto operator=(Rc<T2> const& rhs) noexcept -> Rc& {
        if (this != std::addressof(rhs)) {
            ~Rc();
            inner_ = reinterpret_cast<detail::RcInner<T>*>(rhs.inner_);
            if (inner_) {
                inner_->increase_ref_count();
            }
        }
        return *this;
    }
    template <typename T2> requires std::convertible_to<T2*, T*>
    auto operator=(Rc<T2>&& rhs) noexcept -> Rc& {
        if (this != std::addressof(rhs)) {
            ~Rc();
            inner_ = reinterpret_cast<detail::RcInner<T>*>(rhs.inner_);
            rhs.inner_ = nullptr;
        }
        return *this;
    }

    auto operator==(Rc const& rhs) const -> bool = default;
    auto operator<=>(Rc const& rhs) const = default;

    auto operator*() const noexcept(noexcept(*std::declval<T*>())) -> std::add_lvalue_reference_t<T> {
        return *inner_->ptr;
    }
    auto operator->() const noexcept -> T* { return inner_->ptr; }

    auto get() const -> T* { return inner_->ptr; }

    operator bool() const noexcept { return inner_; }

private:
    template <typename T2>
    friend class Rc;

    Rc(T* ptr) {
        inner_ = new detail::RcInner(ptr);
    }

    detail::RcInner<T>* inner_ = nullptr;
};

}

template <typename T>
struct std::hash<bi::Rc<T>> final {
    auto operator()(bi::Rc<T> const& v) const noexcept -> size_t {
        return std::hash<T*>{}(v.get());
    }
};

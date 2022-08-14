#pragma once

#include <memory>
#include <concepts>

#include "traits.hpp"
#include "logger.hpp"

BISMUTH_NAMESPACE_BEGIN

namespace detail {

// compress space using empty base class
template <typename PT, typename D, bool = std::is_empty_v<D> && !std::is_final_v<D>>
struct PtrInner : private D {
    PT ptr;

    PtrInner(PT ptr) noexcept : ptr(std::move(ptr)) {}
    PtrInner(PT ptr, const D &deleter) noexcept : ptr(std::move(ptr)) {}

    constexpr D &Deleter() noexcept {
        return *this;
    }
    constexpr const D &Deleter() const noexcept {
        return *this;
    }
};

template <typename PT, typename D>
struct PtrInner<PT, D, false> {
    PT ptr;
    D deleter;

    PtrInner(PT ptr) noexcept : ptr(std::move(ptr)), deleter() {}
    PtrInner(PT ptr, const D &deleter) noexcept : ptr(std::move(ptr)), deleter(deleter) {}

    constexpr D &Deleter() noexcept {
        return deleter;
    }
    constexpr const D &Deleter() const noexcept {
        return deleter;
    }
};

}

// a pointer that cannot be null, use raw pointer when it is nullable
template <NonArrayT T>
class Ref final {
public:
    Ref(const Ref &rhs) noexcept : ptr_(rhs.ptr_) {}
    Ref(Ref &&rhs) noexcept : ptr_(rhs.ptr_) {}

    template <NonArrayT T2> requires std::convertible_to<T2 *, T *>
    Ref(const Ref<T2> &rhs) noexcept : ptr_(rhs.ptr_) {}
    template <NonArrayT T2> requires std::convertible_to<T2 *, T *>
    Ref(Ref<T2> &&rhs) noexcept : ptr_(rhs.ptr_) {}

    Ref &operator=(const Ref &rhs) noexcept { ptr_ = rhs.ptr_; return *this; }
    Ref &operator=(Ref &&rhs) noexcept { ptr_ = rhs.ptr_; return *this; }

    template <NonArrayT T2> requires std::convertible_to<T2 *, T *>
    Ref &operator=(const Ref<T2> &rhs) noexcept { ptr_ = rhs.ptr_; return *this; }
    template <NonArrayT T2> requires std::convertible_to<T2 *, T *>
    Ref &operator=(Ref<T2> &&rhs) noexcept { ptr_ = rhs.ptr_; return *this; }

    bool operator==(const Ref &rhs) const = default;
    auto operator<=>(const Ref &rhs) const = default;

    std::add_lvalue_reference_t<T> operator*() const noexcept(noexcept(*std::declval<T *>())) {
        return *ptr_;
    }

    T *operator->() const noexcept {
        return ptr_;
    }

    T *Get() const noexcept {
        return ptr_;
    }

    template <NonArrayT T2> requires requires { static_cast<T2 *>(std::declval<T *>()); }
    Ref<T2> CastTo() const {
        return Ref<T2>(static_cast<T2 *>(ptr_));
    }

private:
    Ref(T *ptr) noexcept : ptr_(ptr) {}

    template <NonArrayT T2>
    friend class Ref;

    template <NonArrayT T2, typename D>
    friend class Ptr;

    template <NonArrayT T2>
    friend class RefFromThis;

    T *ptr_;
};

template <NonArrayT T, typename D = std::default_delete<T>>
class Ptr final {
public:
    Ptr() : ptr_(nullptr) {}

    template <typename... Args> requires std::constructible_from<T, Args...>
    static Ptr Make(Args &&... args) noexcept {
        return Ptr(new T(std::forward<Args>(args)...));
    }

    ~Ptr() noexcept {
        if (ptr_.ptr) {
            ptr_.Deleter()(ptr_.ptr);
            ptr_.ptr = nullptr;
        }
    }

    Ptr(Ptr &&rhs) noexcept : ptr_(rhs.Release(), rhs.ptr_.Deleter()) {}

    template <NonArrayT T2, typename D2> requires std::convertible_to<T2 *, T *>
    Ptr(Ptr<T2, D2> &&rhs) noexcept : ptr_(rhs.Release(), rhs.ptr_.Deleter()) {}

    Ptr &operator=(Ptr &&rhs) noexcept {
        if (this != std::addressof(rhs)) {
            Reset(rhs.Release());
            ptr_.Deleter() = rhs.ptr_.Deleter();
        }
        return *this;
    }

    template <NonArrayT T2, typename D2> requires std::convertible_to<T2 *, T *>
    Ptr &operator=(Ptr<T2, D2> &&rhs) noexcept {
        Reset(rhs.Release());
        ptr_.Deleter() = rhs.ptr_.Deleter();
        return *this;
    }

    bool operator==(const Ptr &rhs) const { return ptr_.ptr == rhs.ptr_.ptr; }
    std::compare_three_way_result_t<T *> operator<=>(const Ptr &rhs) const { return ptr_.ptr <=> rhs.ptr_.ptr; }

    std::add_lvalue_reference_t<T> operator*() const noexcept(noexcept(*std::declval<T *>())) {
        BI_ASSERT_MSG(ptr_.ptr != nullptr, "use uninitialized / moved Ptr");
        return *ptr_.ptr;
    }

    T *operator->() const noexcept {
        BI_ASSERT_MSG(ptr_.ptr != nullptr, "use uninitialized / moved Ptr");
        return ptr_.ptr;
    }

    T *Get() const noexcept {
        return ptr_.ptr;
    }

    Ref<T> AsRef() const noexcept {
        BI_ASSERT_MSG(ptr_.ptr != nullptr, "use uninitialized / moved Ptr");
        return Ref<T>(ptr_.ptr);
    }
    operator Ref<T>() const noexcept {
        BI_ASSERT_MSG(ptr_.ptr != nullptr, "use uninitialized / moved Ptr");
        return Ref<T>(ptr_.ptr);
    }

private:
    explicit Ptr(T *ptr) noexcept : ptr_(ptr) {}

    void Reset(T *ptr) {
        if (ptr_.ptr) {
            ptr_.Deleter()(ptr_.ptr);
        }
        ptr_.ptr = ptr;
    }

    T *Release() {
        BI_ASSERT_MSG(ptr_.ptr != nullptr, "use uninitialized / moved Ptr");
        return std::exchange(ptr_.ptr, nullptr);
    }

    template <NonArrayT T2, typename D2>
    friend class Ptr;

    detail::PtrInner<T *, D> ptr_;
};

template <NonArrayT T>
class RefFromThis {
public:
    Ref<T> RefThis() noexcept {
        return Ref<T>(static_cast<T *>(this));
    }
};

BISMUTH_NAMESPACE_END

template <bismuth::NonArrayT T>
struct std::hash<bismuth::Ref<T>> {
    size_t operator()(bismuth::Ref<T> v) const noexcept {
        std::hash<T *> hasher;
        return hasher(v.Get());
    }
};

template <bismuth::NonArrayT T, typename D>
struct std::hash<bismuth::Ptr<T, D>> {
    size_t operator()(const bismuth::Ptr<T, D> &v) const noexcept {
        std::hash<T *> hasher;
        return hasher(v.Get());
    }
};

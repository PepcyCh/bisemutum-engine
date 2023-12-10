#pragma once

#include "box.hpp"

namespace bi {

template <typename Signature>
struct MoveOnlyFunction;

namespace details {

template <typename Signature>
struct MoveOnlyFunctionImplBase;

template <typename R, typename...Args>
struct MoveOnlyFunctionImplBase<auto(Args...) -> R> {
    virtual ~MoveOnlyFunctionImplBase() = default;
    virtual auto invoke(Args&&...args) const -> R = 0;
};

template <typename F, typename Signature>
struct MoveOnlyFunctionImpl;

template <typename F, typename R, typename... Args>
struct MoveOnlyFunctionImpl<F, auto(Args...) -> R> final : MoveOnlyFunctionImplBase<auto(Args...) -> R> {
    F f;
    template <typename T>
    MoveOnlyFunctionImpl(T&& t) : f(std::forward<T>(t)) {}
    auto invoke(Args&&...args) const -> R override { return f(std::forward<Args>(args)...); }
};

template<typename F, typename...Args>
struct MoveOnlyFunctionImpl<F, auto(Args...) -> void> final : MoveOnlyFunctionImplBase<auto(Args...) -> void> {
    F f;
    template <typename T>
    MoveOnlyFunctionImpl(T&& t) : f(std::forward<T>(t)) {}
    auto invoke(Args&&...args) const -> void override { f(std::forward<Args>(args)...); }
};

} // namespace details

template <typename R, typename... Args>
struct MoveOnlyFunction<auto(Args...) -> R> final {
    MoveOnlyFunction() = default;

    MoveOnlyFunction(MoveOnlyFunction&& rhs) noexcept = default;
    auto operator=(MoveOnlyFunction&& rhs) noexcept -> MoveOnlyFunction& = default;

    auto operator()(Args... args) const -> R {
        return impl_->invoke(std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept { return static_cast<bool>(impl_); }

    struct CtorTag{};
    template<
        typename F,
        typename DecayF = std::decay_t<F>,
        typename = std::enable_if_t<!std::is_same_v<DecayF, MoveOnlyFunction>>,
        typename FR = decltype(std::declval<F const&>()(std::declval<Args>()...)),
        std::enable_if_t<std::is_same_v<R, void> || std::is_convertible_v<FR, R>>* = nullptr,
        std::enable_if_t<std::is_convertible_v<DecayF, bool>>* = nullptr
    >
    MoveOnlyFunction(F&& f)
        : MoveOnlyFunction(
            static_cast<bool>(f) ? MoveOnlyFunction(CtorTag{}, std::forward<F>(f)) : MoveOnlyFunction()
        ) {}

    template<
        typename F,
        typename DecayF = std::decay_t<F>,
        typename = std::enable_if_t<!std::is_same_v<DecayF, MoveOnlyFunction>>,
        typename FR = decltype(std::declval<F const&>()(std::declval<Args>()...)),
        std::enable_if_t<std::is_same_v<R, void> || std::is_convertible_v<FR, R>>* = nullptr,
        std::enable_if_t<!std::is_convertible_v<DecayF, bool>>* = nullptr
    >
    MoveOnlyFunction(F&& f) : MoveOnlyFunction(CtorTag{}, std::forward<F>(f)) {}

    MoveOnlyFunction(std::nullptr_t) : MoveOnlyFunction() {}

    MoveOnlyFunction(R(*pf)(Args...)) : MoveOnlyFunction(pf ? MoveOnlyFunction(CtorTag{}, pf) : MoveOnlyFunction()) {}

private:
    template<typename F, typename DecayF = std::decay_t<F>>
    MoveOnlyFunction(CtorTag, F&& f)
        : impl_(Box<details::MoveOnlyFunctionImpl<DecayF, auto(Args...) -> R>>::make(std::forward<F>(f))) {}

    Box<details::MoveOnlyFunctionImplBase<auto(Args...) -> R>> impl_;
};

}

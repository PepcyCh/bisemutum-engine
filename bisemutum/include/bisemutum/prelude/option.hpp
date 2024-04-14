#pragma once

#include <cstring>
#include <cassert>

#include "traits.hpp"
#include "hash.hpp"

namespace bi {

template <typename T>
struct Option final {
    Option() noexcept = default;

    template <typename... Args> requires std::constructible_from<T, Args...>
    static auto make(Args&&... args) noexcept -> Option {
        Option opt{};
        new (opt.memory_) T(std::forward<Args>(args)...);
        opt.has_value_ = true;
        return opt;
    }

    ~Option() { reset(); }

    Option(T const& value) noexcept : has_value_(true) { new (memory_) T(value); }
    Option(T&& value) noexcept : has_value_(true) { new (memory_) T(std::move(value)); }

    Option(Option<T> const& rhs) noexcept : has_value_(rhs.has_value_) {
        if (has_value_) {
            new (memory_) T(rhs.value());
        }
    }
    Option(Option<T>&& rhs) noexcept : has_value_(rhs.has_value_) {
        if (has_value_) {
            new (memory_) T(std::move(rhs.value()));
            rhs.reset();
        }
    }

    template <typename T2> requires std::convertible_to<T2, T>
    Option(Option<T2> const& rhs) noexcept : has_value_(rhs.has_value_) {
        if (has_value_) {
            new (memory_) T(rhs.value());
        }
    }
    template <typename T2> requires std::convertible_to<T2, T>
    Option(Option<T2>&& rhs) noexcept : has_value_(rhs.has_value_) {
        if (has_value_) {
            new (memory_) T(std::move(rhs.value()));
            rhs.reset();
        }
    }

    auto operator=(T const& value) noexcept -> Option& {
        reset();
        has_value_ = true;
        new (memory_) T(value);
        return *this;
    }
    auto operator=(T&& value) noexcept -> Option& {
        reset();
        has_value_ = true;
        new (memory_) T(std::move(value));
        return *this;
    }

    auto operator=(Option<T> const& rhs) noexcept -> Option& {
        reset();
        has_value_ = rhs.has_value_;
        if (has_value_) {
            new (memory_) T(rhs.value());
        }
        return *this;
    }
    auto operator=(Option<T>&& rhs) noexcept -> Option& {
        reset();
        has_value_ = rhs.has_value_;
        if (has_value_) {
            new (memory_) T(std::move(rhs.value()));
            rhs.reset();
        }
        return *this;
    }

    template <typename T2> requires std::convertible_to<T2, T>
    auto operator=(Option<T2> const& rhs) noexcept -> Option& {
        reset();
        has_value_ = rhs.has_value_;
        if (has_value_) {
            new (memory_) T(rhs.value());
        }
        return *this;
    }
    template <typename T2> requires std::convertible_to<T2, T>
    auto operator=(Option<T2>&& rhs) noexcept -> Option& {
        reset();
        has_value_ = rhs.has_value_;
        if (has_value_) {
            new (memory_) T(std::move(rhs.value()));
            rhs.reset();
        }
        return *this;
    }

    auto operator==(Option const& rhs) const noexcept -> bool {
        return has_value_ ? (rhs.has_value() && value() == rhs.value()) : !rhs.has_value();
    }
    template <typename T2> requires std::convertible_to<T2, T>
    auto operator==(Option<T2> const& rhs) const noexcept -> bool {
        return has_value_ ? (rhs.has_value() && value() == rhs.value()) : !rhs.has_value();
    }

    auto value() & -> T& {
        assert(has_value() && "call 'value()' on empty Option");
        return *reinterpret_cast<T*>(memory_);
    }
    auto value() const& -> T const& {
        assert(has_value() && "call 'value()' on empty Option");
        return *reinterpret_cast<T const*>(memory_);
    }
    auto value() && -> T&& {
        assert(has_value() && "call 'value()' on empty Option");
        return std::move(*reinterpret_cast<T*>(memory_));
    }

    auto value_or(T const& fallback) const -> T const& {
        return has_value_ ? *reinterpret_cast<T const*>(memory_) : fallback;
    }

    auto has_value() const -> bool { return has_value_; }
    operator bool() const { return has_value_; }

    template <typename... Args>
    auto emplace(Args&&... args) -> void {
        reset();
        has_value_ = true;
        new (memory_) T(std::forward<Args>(args)...);
    }

    auto reset() -> void {
        if (has_value_) {
            value().~T();
            has_value_ = false;
        }
    }

private:
    template <typename T2>
    friend class Option;

    alignas(T) std::byte memory_[sizeof(T)];
    bool has_value_ = false;
};

template <>
struct Option<bool> final {
    Option() = default;

    static auto make(bool value) -> Option {
        Option opt {};
        opt.value_ = value ? 1 : 0;
        return opt;
    }

    ~Option() { reset(); }

    Option(bool value) : value_(value ? 1 : 0) {}

    Option(Option<bool> const& rhs) : value_(rhs.value_) {}
    Option(Option<bool>&& rhs) : value_(rhs.value_) { rhs.reset(); }

    template <typename T> requires std::convertible_to<T, bool>
    Option(Option<T> const& rhs) {
        if (rhs.has_value()) {
            value_ = static_cast<bool>(rhs.value()) ? 1 : 0;
        } else {
            value_ = 0xff;
        }
    }
    template <typename T> requires std::convertible_to<T, bool>
    Option(Option<T>&& rhs) {
        if (rhs.has_value()) {
            value_ = static_cast<bool>(rhs.value()) ? 1 : 0;
            rhs.reset();
        } else {
            value_ = 0xff;
        }
    }

    auto operator=(bool value) -> Option& {
        value_ = value ? 1 : 0;
        return *this;
    }

    auto operator=(Option<bool> const& rhs) -> Option& {
        value_ = rhs.value_;
        return *this;
    }
    auto operator=(Option<bool>&& rhs) -> Option& {
        value_ = rhs.value_;
        rhs.reset();
        return *this;
    }

    template <typename T> requires std::convertible_to<T, bool>
    auto operator=(Option<T> const& rhs) -> Option& {
        if (rhs.has_value()) {
            value_ = static_cast<bool>(rhs.value()) ? 1 : 0;
        } else {
            value_ = 0xff;
        }
        return *this;
    }
    template <typename T> requires std::convertible_to<T, bool>
    auto operator=(Option<T>&& rhs) -> Option& {
        if (rhs.has_value()) {
            value_ = static_cast<bool>(rhs.value()) ? 1 : 0;
            rhs.reset();
        } else {
            value_ = 0xff;
        }
        return *this;
    }

    auto operator==(Option const& rhs) const -> bool {
        return has_value() ? (rhs.has_value() && value() == rhs.value()) : !rhs.has_value();
    }
    template <typename T> requires std::convertible_to<T, bool>
    auto operator==(Option<T> const& rhs) const -> bool {
        return has_value() ? (rhs.has_value() && value() == rhs.value()) : !rhs.has_value();
    }

    auto value() -> bool& { return *reinterpret_cast<bool*>(&value_); }
    auto value() const -> bool { return *reinterpret_cast<bool const*>(&value_); }

    auto value_or(bool fallback) const -> bool {
        return has_value() ? *reinterpret_cast<bool const*>(&value_) : fallback;
    }

    auto has_value() const -> bool { return value_ != 0xff; }
    operator bool() const { return value_ != 0xff; }

    auto reset() -> void { value_ = 0xff; }

private:
    uint8_t value_ = 0xff;
};

namespace detail {

constexpr size_t MEMORY_ALL_ZERO_MAX_SIZE = 4096;

template <size_t N>
auto memory_all_zero(std::byte const* memory) -> bool {
    static const std::byte zero[MEMORY_ALL_ZERO_MAX_SIZE] = {};
    if constexpr (N <= MEMORY_ALL_ZERO_MAX_SIZE) {
        return std::memcmp(memory, zero, N) == 0;
    } else {
        size_t i = 0;
        bool all_zero = true;
        for (; i < N; i += MEMORY_ALL_ZERO_MAX_SIZE) {
            all_zero &= std::memcmp(memory + i, zero, MEMORY_ALL_ZERO_MAX_SIZE) == 0;
        }
        all_zero &= std::memcmp(memory + i, zero, N % MEMORY_ALL_ZERO_MAX_SIZE) == 0;
        return all_zero;
    }
}

}

template <typename T> requires traits::nonzeroable_v<T>
struct Option<T> final {
    Option() noexcept { std::memset(memory_, 0, sizeof(T)); }

    template <typename... Args> requires std::constructible_from<T, Args...>
    static auto make(Args&&... args) noexcept -> Option {
        Option opt {};
        new (opt.memory_) T(std::forward<Args>(args)...);
        return opt;
    }

    ~Option() { reset(); }

    Option(T const& value) noexcept { new (memory_) T(value); }
    Option(T&& value) noexcept { new (memory_) T(std::move(value)); }

    Option(Option<T> const& rhs) noexcept {
        if (rhs.has_value()) {
            new (memory_) T(rhs.value());
        }
    }
    Option(Option<T>&& rhs) noexcept {
        if (rhs.has_value()) {
            new (memory_) T(std::move(rhs.value()));
            rhs.reset();
        }
    }

    template <typename T2> requires std::convertible_to<T2, T>
    Option(Option<T2> const& rhs) noexcept {
        if (rhs.has_value()) {
            new (memory_) T(rhs.value());
        }
    }
    template <typename T2> requires std::convertible_to<T2, T>
    Option(Option<T2>&& rhs) noexcept {
        if (rhs.has_value()) {
            new (memory_) T(std::move(rhs.value()));
            rhs.reset();
        }
    }

    auto operator=(T const& value) noexcept -> Option& {
        reset();
        new (memory_) T(value);
        return *this;
    }
    auto operator=(T&& value) noexcept -> Option& {
        reset();
        new (memory_) T(std::move(value));
        return *this;
    }

    auto operator=(Option<T> const& rhs) noexcept -> Option& {
        reset();
        if (rhs.has_value()) {
            new (memory_) T(rhs.value());
        }
        return *this;
    }
    auto operator=(Option<T>&& rhs) noexcept -> Option& {
        reset();
        if (rhs.has_value()) {
            new (memory_) T(std::move(rhs.value()));
            rhs.reset();
        }
        return *this;
    }

    template <typename T2> requires std::convertible_to<T2, T>
    auto operator=(Option<T2> const& rhs) noexcept -> Option& {
        reset();
        if (rhs.has_value()) {
            new (memory_) T(rhs.value());
        }
        return *this;
    }
    template <typename T2> requires std::convertible_to<T2, T>
    auto operator=(Option<T2>&& rhs) noexcept -> Option& {
        reset();
        if (rhs.has_value()) {
            new (memory_) T(std::move(rhs.value()));
            rhs.reset();
        }
        return *this;
    }

    auto operator==(Option const& rhs) const noexcept -> bool {
        return has_value() ? (rhs.has_value() && value() == rhs.value()) : !rhs.has_value();
    }
    template <typename T2> requires std::convertible_to<T2, T>
    auto operator==(Option<T2> const& rhs) const noexcept -> bool {
        return has_value() ? (rhs.has_value() && value() == rhs.value()) : !rhs.has_value();
    }

    auto value() & -> T& {
        assert(has_value() && "call 'value()' on empty Option");
        return *reinterpret_cast<T*>(memory_);
    }
    auto value() const& -> T const& {
        assert(has_value() && "call 'value()' on empty Option");
        return *reinterpret_cast<T const*>(memory_);
    }
    auto value() && -> T&& {
        assert(has_value() && "call 'value()' on empty Option");
        return std::move(*reinterpret_cast<T*>(memory_));
    }

    auto value_or(T const& fallback) const -> T const& {
        return has_value() ? *reinterpret_cast<T const*>(memory_) : fallback;
    }

    auto has_value() const -> bool { return detail::memory_all_zero<sizeof(T)>(memory_); }
    operator bool() const { return has_value(); }

    auto reset() -> void {
        if (has_value()) {
            value().~T();
            std::memset(memory_, 0, sizeof(T));
        }
    }

    template <typename... Args>
    auto emplace(Args&&... args) -> void {
        reset();
        new (memory_) T(std::forward<Args>(args)...);
    }

private:
    alignas(T) std::byte memory_[sizeof(T)];
};

}

template <typename T>
struct std::hash<bi::Option<T>> final {
    auto operator()(bi::Option<T> const& v) const noexcept -> size_t {
        auto has_value = v.has_value();
        size_t hash = std::hash<bool>{}(has_value);
        if (has_value) {
            hash = bi::hash_combine(hash, v.value());
        }
        return hash;
    }
};

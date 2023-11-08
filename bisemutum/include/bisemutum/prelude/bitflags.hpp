#pragma once

#include <initializer_list>

#include "traits.hpp"

namespace bi {

template <traits::Enum E>
struct BitFlags final {
    using ValueType = std::underlying_type_t<E>;

    constexpr BitFlags() noexcept = default;
    constexpr BitFlags(E value) noexcept : value_(static_cast<ValueType>(value)) {}
    constexpr BitFlags(std::initializer_list<E> list) noexcept : value_(0) {
        for (E item : list) {
            value_ |= static_cast<ValueType>(item);
        }
    }

    constexpr auto operator=(E rhs) noexcept -> BitFlags<E>& {
        value_ = static_cast<ValueType>(rhs);
        return *this;
    }
    constexpr auto operator=(std::initializer_list<E> list) noexcept -> BitFlags<E>& {
        value_ = 0;
        for (E item : list) {
            value_ |= static_cast<ValueType>(item);
        }
        return *this;
    }

    constexpr auto set(BitFlags<E> rhs) noexcept -> BitFlags<E>& {
        value_ |= rhs.value_;
        return *this;
    }
    constexpr auto clear(BitFlags<E> rhs) noexcept -> BitFlags<E>& {
        value_ &= ~rhs.value_;
        return *this;
    }
    constexpr auto contains_any(BitFlags<E> rhs) const noexcept -> bool {
        return rhs.value_ == 0 || (value_ & rhs.value_) != 0;
    }
    constexpr auto contains_all(BitFlags<E> rhs) const noexcept -> bool {
        return (value_ & rhs.value_) == rhs.value_;
    }

    constexpr auto raw_value() const noexcept -> ValueType { return value_; }

    constexpr auto operator==(BitFlags<E> const& rhs) const noexcept -> bool = default;

private:
    ValueType value_ = 0;
};

}

template <bi::traits::Enum E>
struct std::hash<bi::BitFlags<E>> final {
    auto operator()(bi::BitFlags<E> const& v) const noexcept -> size_t {
        return std::hash<decltype(v.raw_value())>{}(v.raw_value());
    }
};

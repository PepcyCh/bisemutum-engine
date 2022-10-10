#pragma once

#include "traits.hpp"

BISMUTH_NAMESPACE_BEGIN

template <EnumT E>
class BitFlags {
    using ValueType = std::underlying_type_t<E>;

public:
    BitFlags() : value_(0) {}
    BitFlags(E value) : value_(static_cast<ValueType>(value)) {}
    BitFlags(std::initializer_list<E> list) : value_(0) {
        for (E item : list) {
            value_ |= static_cast<ValueType>(item);
        }
    }
    
    BitFlags<E> operator=(E rhs) {
        value_ = static_cast<ValueType>(rhs);
    }
    BitFlags<E> operator=(std::initializer_list<E> list) {
        value_ = 0;
        for (E item : list) {
            value_ |= static_cast<ValueType>(item);
        }
    }

    BitFlags<E> &Set(BitFlags<E> rhs) {
        value_ |= rhs.value_;
        return *this;
    }
    BitFlags<E> &Clear(BitFlags<E> rhs) {
        value_ &= ~rhs.value_;
        return *this;
    }
    bool Contains(BitFlags<E> rhs) const {
        return (value_ & rhs.value_) != 0;
    }

    ValueType RawValue() const {
        return value_;
    }

    bool operator==(const BitFlags<E> &rhs) const = default;

private:
    ValueType value_;
};

BISMUTH_NAMESPACE_END

template <bismuth::EnumT E>
struct std::hash<bismuth::BitFlags<E>> {
    size_t operator()(const bismuth::BitFlags<E> &v) const noexcept {
        std::hash<decltype(v.RawValue())> hasher;
        return hasher(v.RawValue());
    }
};

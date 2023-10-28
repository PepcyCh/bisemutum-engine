#pragma once

#include <variant>
#include <string>
#include <vector>
#include <array>

#include "srefl.hpp"
#include "../prelude/option.hpp"
#include "../containers/hash.hpp"

namespace bi::serde {

struct Exception final : std::exception {
    Exception() noexcept = default;
    Exception(std::string const& str) noexcept : error_(str) {}
    Exception(std::string&& str) noexcept : error_(std::move(str)) {}

    auto what() const -> char const* override { return error_.c_str(); }

private:
    std::string error_;
};

struct Value final {
    using Bool = bool;
    using Float = double;
    using Integer = int64_t;
    using String = std::string;
    using Array = std::vector<Value>;
    using Table = StringHashMap<Value>;

    Value() noexcept = default;
    Value(Bool v) noexcept : value_(v) {}
    Value(Float v) noexcept : value_(v) {}
    Value(Integer v) noexcept : value_(v) {}
    Value(String const& v) noexcept : value_(v) {}
    Value(String&& v) noexcept : value_(std::move(v)) {}
    Value(Array const& v) noexcept : value_(v) {}
    Value(Array&& v) noexcept : value_(std::move(v)) {}
    Value(Table const& v) noexcept : value_(v) {}
    Value(Table&& v) noexcept : value_(std::move(v)) {}

    // Add 'noexcept'
    Value(Value const& v) noexcept = default;
    Value(Value&& v) noexcept = default;
    auto operator=(Value const& v) noexcept -> Value& = default;
    auto operator=(Value&& v) noexcept -> Value& = default;

    auto is_bool() const noexcept -> bool { return value_.index() == 1; }
    auto is_float() const noexcept -> bool { return value_.index() == 2; }
    auto is_integer() const noexcept -> bool { return value_.index() == 3; }
    auto is_string() const noexcept -> bool { return value_.index() == 4; }
    auto is_array() const noexcept -> bool { return value_.index() == 5; }
    auto is_table() const noexcept -> bool { return value_.index() == 6; }

    template <typename T>
    auto get() const -> T;
    template <typename T>
    auto get_ref() const -> T const&;

    template <typename T>
    auto try_get() const -> Option<T>;
    template <typename T>
    auto try_get_ref() const -> T const*;

    template <typename T>
    auto value(size_t index, T const& fallback) const -> T;
    template <typename T>
    auto value(std::string const& key, T const& fallback) const -> T;
    template <typename T>
    auto value(std::string_view key, T const& fallback) const -> T;
    template <typename T>
    auto value(char const* key, T const& fallback) const -> T;

    auto operator[](size_t index) const -> Value const&;
    auto operator[](std::string const& key) const -> Value const&;
    auto operator[](std::string_view key) const -> Value const&;
    auto operator[](char const* key) const -> Value const&;

    auto operator[](size_t index) -> Value&;
    auto operator[](std::string const& key) -> Value&;
    auto operator[](std::string_view key) -> Value&;
    auto operator[](char const* key) -> Value&;

    auto at(size_t index) const -> Value const&;
    auto at(std::string const& key) const -> Value const&;
    auto at(std::string_view key) const -> Value const&;
    auto at(char const* key) const -> Value const&;

    auto try_at(size_t index) const -> Value const*;
    auto try_at(std::string const& key) const -> Value const*;
    auto try_at(std::string_view key) const -> Value const*;
    auto try_at(char const* key) const -> Value const*;

    auto size() const -> size_t;

    auto contains(std::string const& key) const -> bool;
    auto contains(std::string_view key) const -> bool;
    auto contains(char const* key) const -> bool;

    template <typename T>
    auto push_back(T const& v) -> void {
        if (value_.index() == 0) { value_ = Array{}; }
        std::get<Array>(value_).push_back(v);
    }
    template <typename T>
    auto push_back(T&& v) -> void {
        if (value_.index() == 0) { value_ = Array{}; }
        std::get<Array>(value_).emplace_back(std::move(v));
    }

    auto variant() const -> auto const& { return value_; }

    auto to_json(int indent = -1) const -> std::string;
    auto to_toml() const -> std::string;

    static auto from_json(std::string_view json_str) -> Value;
    static auto from_toml(std::string_view toml_str) -> Value;

private:
    std::variant<std::monostate, Bool, Float, Integer, String, Array, Table> value_;
};

template <typename T>
auto to_value(Value &v, T const& o) -> void;
template <typename T>
auto from_value(Value const& v, T& o) -> void;

template <typename T>
auto Value::get() const -> T {
    if constexpr (traits::OneOf<T, Bool, Float, Integer, String, Array, Table>) {
        return std::get<T>(value_);
    } else {
        T o{};
        from_value(*this, o);
        return o;
    }
}
template <typename T>
auto Value::get_ref() const -> T const& {
    return std::get<T>(value_);
}
template <typename T>
auto Value::try_get() const -> Option<T> {
    try {
        return get<T>();
    } catch (std::exception const& e) {
        return {};
    }
}
template <typename T>
auto Value::try_get_ref() const -> T const* {
    return std::get_if<T>(&value_);
}

template <typename T>
auto Value::value(size_t index, T const& fallback) const -> T {
    if (auto arr = std::get_if<Array>(&value_); arr && index < arr->size()) {
        return (*arr)[index].template get<T>();
    } else {
        return fallback;
    }
}
template <typename T>
auto Value::value(std::string const& key, T const& fallback) const -> T {
    if (auto table = std::get_if<Table>(&value_); table) {
        if (auto it = table->find(key); it != table->end()) {
            return it->second.template get<T>();
        }
    }
    return fallback;
}
template <typename T>
auto Value::value(std::string_view key, T const& fallback) const -> T {
    if (auto table = std::get_if<Table>(&value_); table) {
        if (auto it = table->find(key); it != table->end()) {
            return it->second.template get<T>();
        }
    }
    return fallback;
}
template <typename T>
auto Value::value(char const* key, T const& fallback) const -> T {
    if (auto table = std::get_if<Table>(&value_); table) {
        if (auto it = table->find(key); it != table->end()) {
            return it->second.template get<T>();
        }
    }
    return fallback;
}

template <typename T>
auto to_value(Value &v, T const& o) -> void {
    if constexpr (requires { T::to_value(v, o); }) {
        T::to_value(v, o);
    } else {
        constexpr auto type = srefl::refl<T>();
        srefl::for_each(type.members, [&v, &o](auto member) {
            to_value(v[member.name], member(o));
        });
    }
}
template <typename T>
auto from_value(Value const& v, T& o) -> void {
    if constexpr (requires { T::from_value(v, o); }) {
        T::from_value(v, o);
    } else {
        constexpr auto type = srefl::refl<T>();
        T fallback{};
        srefl::for_each(type.members, [&v, &o, &fallback](auto member) {
            member(o) = v.value(member.name, member(fallback));
        });
    }
}

template <>
inline auto to_value(Value &v, bool const& o) -> void {
    v = o;
}
template <>
inline auto from_value(Value const& v, bool& o) -> void {
    o = v.get<Value::Bool>();
}

template <std::integral T>
auto to_value(Value &v, T const& o) -> void {
    v = static_cast<Value::Integer>(o);
}
template <std::integral T>
auto from_value(Value const& v, T& o) -> void {
    o = v.get<Value::Integer>();
}

template <std::floating_point T>
auto to_value(Value &v, T const& o) -> void {
    v = static_cast<Value::Float>(o);
}
template <std::floating_point T>
auto from_value(Value const& v, T& o) -> void {
    o = v.get<Value::Float>();
}

template <traits::Enum T>
auto to_value(Value &v, T const& o) -> void {
    v = std::string{magic_enum::enum_name(o)};
}
template <traits::Enum T>
auto from_value(Value const& v, T& o) -> void {
    o = magic_enum::enum_cast<T>(v.get<Value::String>()).value();
}

template <typename T>
auto to_value(Value &v, std::vector<T> const& o) -> void {
    Value::Array arr(o.size());
    for (size_t i = 0; i < o.size(); i++) {
        to_value(arr[i], o[i]);
    }
    v = std::move(arr);
}
template <typename T>
auto from_value(Value const& v, std::vector<T>& o) -> void {
    auto& arr = v.get_ref<Value::Array>();
    o.resize(arr.size());
    for (size_t i = 0; i < arr.size(); i++) {
        from_value(arr[i], o[i]);
    }
}

template <typename T, size_t N>
auto to_value(Value &v, std::array<T, N> const& o) -> void {
    Value::Array arr(o.size());
    for (size_t i = 0; i < o.size(); i++) {
        to_value(arr[i], o[i]);
    }
    v = std::move(arr);
}
template <typename T, size_t N>
auto from_value(Value const& v, std::array<T, N>& o) -> void {
    auto& arr = v.get_ref<Value::Array>();
    if (arr.size() != o.size()) {
        throw Exception{"in from_value<std::array<>>(), size of array in Value must be the same as that of std::array"};
    }
    for (size_t i = 0; i < o.size(); i++) {
        from_value(arr[i], o[i]);
    }
}

}

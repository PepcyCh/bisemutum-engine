#pragma once

#include <variant>

namespace bi {

template <typename T, typename E>
struct Expected final {
    Expected(T const& value) : data_(value) {}
    Expected(T&& value) : data_(std::move(value)) {}
    Expected(E const& error) : data_(error) {}
    Expected(E&& error) : data_(std::move(error)) {}

    auto has_value() const { return data_.index() == 0; }
    operator bool() const { return has_value(); }

    auto value() & -> T& { return std::get<T>(data_); }
    auto value() const& -> T const& { return std::get<T>(data_); }
    auto value() && -> T&& { return std::move(std::get<T>(data_)); }

    auto error() & -> E& { return std::get<E>(data_); }
    auto error() const& -> E const& { return std::get<E>(data_); }
    auto error() && -> E&& { return std::move(std::get<E>(data_)); }

    auto value_or(T const& fallback) const& -> T const& { return has_value() ? value() : fallback; }

private:
    std::variant<T, E> data_;
};

}

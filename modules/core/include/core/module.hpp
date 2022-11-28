#pragma once

#include <concepts>

#include "log.hpp"

BISMUTH_NAMESPACE_BEGIN

class Module {
public:
    virtual ~Module() = default;

    virtual Logger Lgr() const = 0;
};

template <typename T>
concept IsModule = std::derived_from<T, Module>
    && std::default_initializable<T>
    && std::convertible_to<decltype(T::kName), const char *>;

BISMUTH_NAMESPACE_END

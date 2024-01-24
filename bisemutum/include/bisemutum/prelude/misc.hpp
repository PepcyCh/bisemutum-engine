#pragma once

#include <utility>
#include <cstdlib>

namespace bi {

[[noreturn]] inline void unreachable() { std::abort(); }

template <typename... Ts>
struct FunctorsHelper final : Ts... {
    FunctorsHelper(Ts&&... ts) : Ts(std::forward<Ts>(ts))... {}
    using Ts::operator()...;
};
template <typename... Ts>
FunctorsHelper(Ts...) -> FunctorsHelper<Ts...>;

}

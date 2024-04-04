#pragma once

#include <utility>
#include <cstdlib>
#include <cctype>

namespace bi {

[[noreturn]] inline void unreachable() { std::abort(); }

template <typename... Ts>
struct FunctorsHelper final : Ts... {
    FunctorsHelper(Ts&&... ts) : Ts(std::forward<Ts>(ts))... {}
    using Ts::operator()...;
};
template <typename... Ts>
FunctorsHelper(Ts...) -> FunctorsHelper<Ts...>;

inline auto is_identifier_ch(char ch) -> bool {
    return std::isalnum(ch) || ch == '_';
}

}

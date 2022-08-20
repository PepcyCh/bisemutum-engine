#pragma once

#include <variant>

#include "bismuth.hpp"

BISMUTH_NAMESPACE_BEGIN

template <typename... Fs>
static auto MatchVariant(const auto &v, Fs... funcs) {
    struct VisitHelper : Fs... {
        explicit VisitHelper(Fs... fs) : Fs(fs)... {}
        using Fs::operator()...;
    };

    return std::visit(VisitHelper { funcs... }, v);
}

template <typename... Ts>
class Variant : public std::variant<Ts...> {
public:
    using std::variant<Ts...>::variant;

    template <typename... Fs>
    auto Match(Fs... funcs) const {
        return MatchVariant(*this, std::move(funcs)...);
    }
};

BISMUTH_NAMESPACE_END

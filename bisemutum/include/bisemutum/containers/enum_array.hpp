#pragma once

#include <array>

#include "../prelude/traits.hpp"

namespace bi {

template <traits::Enum E, typename V, size_t max_enum_value>
struct EnumArray final {
    constexpr auto size() const -> size_t { return max_enum_value; }

    auto operator[](E key) -> V& { return elems_[static_cast<size_t>(key)]; }
    auto operator[](E key) const -> V const& { return elems_[static_cast<size_t>(key)]; }

    auto data() -> V* { return elems_.data(); }
    auto data() const -> V const* { return elems_.data(); }

private:
    std::array<V, max_enum_value> elems_;
};

}

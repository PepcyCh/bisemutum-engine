#pragma once

#include <concepts>

namespace bi {

template <std::integral I>
auto ceil_div(I a, I b) -> I {
    return (a + b - 1) / b;
}

template <std::integral I>
auto aligned_size(I size, I alignment) -> I {
    return alignment == 0 ? size : ceil_div(size, alignment) * alignment;
}

}

#pragma once

#include <cstdint>

namespace bi {

struct WindowSize final {
    uint32_t width = 0;
    uint32_t height = 0;

    auto operator==(WindowSize const& rhs) const -> bool = default;
};

}

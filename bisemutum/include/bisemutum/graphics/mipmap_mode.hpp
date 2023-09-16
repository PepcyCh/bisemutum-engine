#pragma once

#include <cstdint>

namespace bi::gfx {

enum class MipmapMode : uint8_t {
    average,
    min,
    max,
};

}

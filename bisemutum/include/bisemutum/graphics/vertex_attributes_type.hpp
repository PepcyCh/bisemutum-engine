#pragma once

#include <cstdint>

namespace bi::gfx {

enum class VertexAttributesType : uint8_t {
    none,
    position_only,
    position_texcoord,
    full,
};

}

#pragma once

#include <cstdint>

namespace bi::gfx {

enum class VertexAttributesType : uint8_t {
    none = 0,
    position = 0x1,
    history_position = 0x2,
    normal = 0x4,
    tangent = 0x8,
    color = 0x10,
    texcoord = 0x20,
    texcoord2 = 0x40,
    position_only = position,
    position_texcoord = position | texcoord,
    full = position | history_position | normal | tangent | color | texcoord,
};

}

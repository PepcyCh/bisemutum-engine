#pragma once

#include <cstdint>

namespace bi::gfx {

struct DrawableSbtData final {
    uint32_t drawable_index;
    uint32_t position_offset;
    uint32_t normal_offset;
    uint32_t tangent_offset;
    uint32_t color_offset;
    uint32_t texcoord_offset;
    uint32_t texcoord2_offset;
    uint32_t index_offset;
    uint32_t material_offset;
};

}

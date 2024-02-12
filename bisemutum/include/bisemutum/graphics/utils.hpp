#pragma once

#include <array>

#include "../math/math.hpp"

namespace bi::gfx {

inline constexpr std::array<float3, 6> cubemap_front_directions{
    float3{1.0f, 0.0f, 0.0f},
    float3{-1.0f, 0.0f, 0.0f},
    float3{0.0f, 1.0f, 0.0f},
    float3{0.0f, -1.0f, 0.0f},
    float3{0.0f, 0.0f, 1.0f},
    float3{0.0f, 0.0f, -1.0f},
};

inline constexpr std::array<float3, 6> cubemap_up_directions{
    float3{0.0f, -1.0f, 0.0f},
    float3{0.0f, -1.0f, 0.0f},
    float3{0.0f, 0.0f, 1.0f},
    float3{0.0f, 0.0f, -1.0f},
    float3{0.0f, -1.0f, 0.0f},
    float3{0.0f, -1.0f, 0.0f},
};

}

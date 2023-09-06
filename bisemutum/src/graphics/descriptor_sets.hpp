#pragma once

#include <cstdint>

namespace bi::gfx {

inline constexpr uint32_t graphics_set_mesh = 0;
inline constexpr uint32_t graphics_set_material = 1;
inline constexpr uint32_t graphics_set_fragment = 2;
inline constexpr uint32_t graphics_set_camera = 3;
inline constexpr uint32_t graphics_set_samplers = 3;

inline constexpr uint32_t samplers_binding_shift = 32;

inline constexpr uint32_t possible_max_sets = 8;

}

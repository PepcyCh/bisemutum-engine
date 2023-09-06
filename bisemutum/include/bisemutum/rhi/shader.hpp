#pragma once

#include <string_view>

#include <bisemutum/prelude/ref.hpp>
#include <bisemutum/prelude/span.hpp>

namespace bi::rhi {

enum class ShaderStage : uint16_t {
    vertex = 0x1,
    tessellation_control = 0x2,
    tessellation_evaluation = 0x4,
    geometry = 0x8,
    fragment = 0x10,
    compute = 0x20,
    ray_generation = 0x40,
    ray_miss = 0x80,
    ray_closest_hit = 0x100,
    ray_any_hit = 0x200,
    ray_intersection = 0x400,
    ray_callable = 0x800,
    task = 0x1000,
    mesh = 0x2000,

    all_graphics = vertex | tessellation_control | tessellation_evaluation | geometry | fragment,
    all_ray_tracing = ray_generation | ray_miss | ray_closest_hit | ray_any_hit | ray_intersection | ray_callable,
    all_meshlet = task | mesh,
    all_stages = all_graphics | all_ray_tracing | all_meshlet | compute,
};

struct ShaderModuleDesc final {
    CSpan<std::byte> binary_data;
};

struct ShaderModule {
    virtual ~ShaderModule() = default;
};

}

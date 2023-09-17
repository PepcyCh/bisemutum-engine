#pragma once

#include <bisemutum/math/math.hpp>
#include <bisemutum/graphics/resource.hpp>

namespace bi {

struct LightData final {
    float3 emission;
    float range_sqr;
    float3 position;
    float cos_inner;
    float3 direction;
    float cos_outer;
};

struct LightsContext final {
    auto collect_all_lights() -> void;

    std::vector<LightData> dir_lights;
    gfx::Buffer dir_lights_buffer;
    std::vector<LightData> point_lights;
    gfx::Buffer point_lights_buffer;
    std::vector<LightData> spot_lights;
    gfx::Buffer spot_lights_buffer;
};

}

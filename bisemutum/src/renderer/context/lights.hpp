#pragma once

#include <bisemutum/math/math.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/camera.hpp>

namespace bi {

struct LightData final {
    float3 emission = float3{0.0f};
    float range_sqr = 1.0f;
    float3 position = float3{0.0f};
    float cos_inner = 1.0f;
    float3 direction = float3{0.0f, 0.0f, 1.0f};
    float cos_outer = 0.0f;
};

struct DirLightData final {
    float3 emission = float3{0.0f};
    int sm_index = -1;
    float3 direction = float3{0.0f, 0.0f, 1.0f};
    float shadow_strength = 1.0f;
    float4 cascade_shadow_radius_sqr = float4{0.0f};
    float shadow_bias_factor = 0.001f;
    float _pad1;
    float2 _pad2;
};

struct ShadowDirLight final {
    gfx::Camera camera;
    float shadow_size;
};

struct LightsContext final {
    LightsContext();

    auto collect_all_lights() -> void;

    auto prepare_dir_lights_per_camera(gfx::Camera const& camera) -> void;

    std::vector<DirLightData> dir_lights;
    gfx::Buffer dir_lights_buffer;
    std::vector<LightData> point_lights;
    gfx::Buffer point_lights_buffer;
    std::vector<LightData> spot_lights;
    gfx::Buffer spot_lights_buffer;

    std::vector<ShadowDirLight> dir_lights_with_shadow;
    gfx::Texture dir_lights_shadow_map;
    std::vector<float4x4> dir_lights_shadow_transform;
    gfx::Buffer dir_lights_shadow_transform_buffer;

    Ptr<gfx::Sampler> shadow_map_sampler;
};

}

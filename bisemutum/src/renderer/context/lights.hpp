#pragma once

#include <bisemutum/math/math.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/graphics/handles.hpp>

namespace bi {

struct DirLightData final {
    float3 emission = float3{0.0f};
    int sm_index = -1;
    // direction that points to the light
    float3 direction = float3{0.0f, 0.0f, 1.0f};
    float shadow_strength = 1.0f;
    float4 cascade_shadow_radius_sqr = float4{0.0f};
    float shadow_depth_bias = 0.001f;
    float shadow_normal_bias = 0.0f;
    float2 _pad1;
};
struct PointLightData final {
    float3 emission = float3{0.0f};
    float range_sqr_inv = 1.0f;
    float3 position = float3{0.0f};
    float cos_inner = 1.0f;
    // direction that points to the light
    float3 direction = float3{0.0f, 0.0f, 1.0f};
    float cos_outer = 0.0f;
    int sm_index = -1;
    float shadow_strength = 1.0f;
    float shadow_depth_bias = 0.001f;
    float shadow_normal_bias = 0.0f;
};

struct ShadowLight final {
    gfx::Camera camera;
    float shadow_size;
};

struct ShadowMapTextures final {
    gfx::TextureHandle dir_lights_shadow_map;
    gfx::TextureHandle point_lights_shadow_map;
};

struct LightsContext final {
    LightsContext();

    auto collect_all_lights() -> void;

    auto prepare_dir_lights_per_camera(gfx::Camera const& camera) -> void;

    std::vector<DirLightData> dir_lights;
    gfx::Buffer dir_lights_buffer;
    std::vector<PointLightData> point_lights;
    gfx::Buffer point_lights_buffer;

    std::vector<ShadowLight> dir_lights_with_shadow;
    gfx::Texture dir_lights_shadow_map;
    std::vector<float4x4> dir_lights_shadow_transform;
    gfx::Buffer dir_lights_shadow_transform_buffer;

    std::vector<ShadowLight> point_lights_with_shadow;
    gfx::Texture point_lights_shadow_map;
    std::vector<float4x4> point_lights_shadow_transform;
    gfx::Buffer point_lights_shadow_transform_buffer;

    Ptr<gfx::Sampler> shadow_map_sampler;
};

}

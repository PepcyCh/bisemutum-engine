#pragma once

struct DirLightData {
    float3 emission;
    int sm_index;
    // direction that points to the light
    float3 direction;
    float shadow_strength;
    float4 cascade_shadow_radius_sqr;
    float shadow_depth_bias;
    float shadow_normal_bias;

    float2 _pad1;
};

struct PointLightData {
    float3 emission;
    float range_sqr_inv;
    float3 position;
    float cos_inner;
    // direction that points to the light
    float3 direction;
    float cos_outer;
    int sm_index;
    float shadow_strength;
    float shadow_depth_bias;
    float shadow_normal_bias;
};

#pragma once

struct DirLightData {
    float3 emission;
    int sm_index;
    float3 direction;
    float shadow_strength;
    float4 cascade_shadow_radius_sqr;
    float shadow_bias_factor;

    float _pad1;
    float2 _pad2;
};

struct PointLightData {
    float3 emission;
    float range_sqr_inv;
    float3 position;
    float cos_inner;
    float3 direction;
    float cos_outer;
    int sm_index;
    float shadow_bias_factor;

    float2 _pad1;
};

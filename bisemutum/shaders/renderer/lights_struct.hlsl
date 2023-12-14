#pragma once

struct LightData {
    float3 emission;
    float range_sqr;
    float3 position;
    float cos_inner;
    float3 direction;
    float cos_outer;
};

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

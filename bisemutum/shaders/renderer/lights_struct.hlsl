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

struct LtcLuts {
    Texture3D matrix_lut0;
    Texture3D matrix_lut1;
    Texture3D matrix_lut2;
    Texture3D norm_lut;
    SamplerState sampler;
};

struct RectLightData {
    float3 emission;
    int texture_index;
    float3 center_position;
    uint two_sided;
    float3 position0;
    float _pad1;
    float3 position1;
    float _pad2;
    float3 position2;
    float _pad3;
    float3 position3;
    float _pad4;
};

#pragma once

#include <bisemutum/shaders/core/utils/projection.hlsl>
#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>

#include "utils.hlsl"

#define NUM_POISSON_SAMPLE 8

static const float3 poisson_disk_samples[NUM_POISSON_SAMPLE] = {
    float3(-1.00,  0.00, 1.0),
    float3( 0.00,  1.00, 1.0),
    float3( 1.00,  0.00, 1.0),
    float3( 0.00, -1.00, 1.0),
    float3(-0.25 * SQRT_2,  0.25 * SQRT_2, 0.5),
    float3( 0.25 * SQRT_2,  0.25 * SQRT_2, 0.5),
    float3( 0.25 * SQRT_2, -0.25 * SQRT_2, 0.5),
    float3(-0.25 * SQRT_2, -0.25 * SQRT_2, 0.5),
};

float get_gaussian_weight(float r) {
    return exp(-0.66 * r * r);
}

float calc_blur_radius(float roughness, float max_radius) {
    // return max_radius * get_specular_magic_curve2(roughness, 0.987);
    return max_radius * get_specular_magic_curve2(roughness, 0.75);
}

struct BilateralData {
    float3 position;
    float3 normal;
    float z_01;
    float z_nf;
    float roughness;
};

BilateralData tap_bilateral_data(
    int2 pixel_coord,
    float2 uv,
    Texture2D depth_tex,
    Texture2D normal_roughness_tex,
    float4x4 matrix_inv_proj,
    float4x4 matrix_inv_view
) {
    BilateralData data;

    float depth = depth_tex.Load(int3(pixel_coord, 0)).x;
    data.z_01 = get_linear_01_depth(depth, matrix_inv_proj);
    data.z_nf = get_linear_depth(depth, matrix_inv_proj);
    data.position = position_world_from_depth(uv, depth, matrix_inv_proj, matrix_inv_view);
    float4 normal_roughness = normal_roughness_tex.Load(int3(pixel_coord, 0));
    data.normal = oct_decode(normal_roughness.xy);
    data.roughness = normal_roughness.w;

    return data;
}

float calc_bilateral_weight(BilateralData center, BilateralData tap) {
    const float w_depth = max(0.0, 1.0 - abs(tap.z_01 - center.z_01));

    float normal_closeness = max(0.0, dot(tap.normal, center.normal));
    normal_closeness *= normal_closeness;
    normal_closeness *= normal_closeness;
    const float normal_error = 1.0 - normal_closeness;
    const float w_normal = max(0.0, (1.0 - normal_error));

    const float3 dq = center.position - tap.position;
    const float dist_sqr = dot(dq, dq);
    const float plane_error = max(abs(dot(dq, tap.normal)), abs(dot(dq, center.normal)));
    const float w_plane = (dist_sqr < 0.0001) ? 1.0 : pow(max(0.0, 1.0 - 2.0 * plane_error / sqrt(dist_sqr)), 2.0);

    const float w_roughness = max(0.0, 1.0 - abs(tap.roughness - center.roughness));

    return w_depth * w_normal * w_plane * w_roughness;
}

#pragma once

#include "utils.hlsl"

float3 surface_eval_unlit(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface
) {
    return surface.base_color;
}

float3 surface_eval_ibl_unlit(
    float3 N,
    float3 V,
    SurfaceData surface,
    float3 ibl_diffuse,
    float3 ibl_specular,
    float2 ibl_brdf
) {
    return float3(0.0, 0.0, 0.0);
}

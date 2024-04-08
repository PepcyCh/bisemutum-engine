#pragma once

#include "utils.hlsl"

void surface_eval_unlit(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface,
    out float3 diffuse_result,
    out float3 specular_result
) {
    diffuse_result = surface.base_color;
    specular_result = 0.0;
}

void surface_eval_lut_unlit(
    float3 N,
    float3 V,
    SurfaceData surface,
    float3 integrated_diffuse,
    float3 integrated_specular,
    float2 integrated_brdf,
    out float3 diffuse_result,
    out float3 specular_result
) {
    diffuse_result = 0.0;
    specular_result = 0.0;
}

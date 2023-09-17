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

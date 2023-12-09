#pragma once

#define MATERIAL_SURFACE_MODEL_UNLIT 0
#define MATERIAL_SURFACE_MODEL_LIT 1
#define MATERIAL_SURFACE_MODEL_NONE 0xffffffffu

#ifndef MATERIAL_SURFACE_MODEL
#define MATERIAL_SURFACE_MODEL MATERIAL_SURFACE_MODEL_NONE
#endif

#include "material/utils.hlsl"

#if MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_UNLIT

#include "material/unlit.hlsl"

#elif MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_LIT

#include "material/lit.hlsl"

#endif

float3 surface_eval(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface
) {
#if MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_UNLIT
    return surface_eval_unlit(N, T, B, V, L, surface);
#elif MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_LIT
    return surface_eval_lit(N, T, B, V, L, surface);
#else
    return float3(0.0, 0.0, 0.0);
#endif
}

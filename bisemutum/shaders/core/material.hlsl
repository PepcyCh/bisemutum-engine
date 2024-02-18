#pragma once

#define MATERIAL_SURFACE_MODEL_UNLIT 0
#define MATERIAL_SURFACE_MODEL_LIT 1
#define MATERIAL_SURFACE_MODEL_NONE 0xffffffffu

#ifndef MATERIAL_SURFACE_MODEL
#define MATERIAL_SURFACE_MODEL MATERIAL_SURFACE_MODEL_NONE
#endif

#include "material/utils.hlsl"
#include "material/unlit.hlsl"
#include "material/lit.hlsl"

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

float3 surface_eval_lut(
    float3 N,
    float3 V,
    SurfaceData surface,
    float3 integrated_diffuse,
    float3 integrated_specular,
    float2 integrated_brdf
) {
#if MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_UNLIT
    return surface_eval_lut_unlit(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf);
#elif MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_LIT
    return surface_eval_lut_lit(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf);
#else
    return float3(0.0, 0.0, 0.0);
#endif
}

float3 surface_eval(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface,
    uint surface_model
) {
    switch (surface_model) {
        case MATERIAL_SURFACE_MODEL_UNLIT:
            return surface_eval_unlit(N, T, B, V, L, surface);
        case MATERIAL_SURFACE_MODEL_LIT:
            return surface_eval_lit(N, T, B, V, L, surface);
        default:
            return float3(0.0, 0.0, 0.0);
    }
}

float3 surface_eval_lut(
    float3 N,
    float3 V,
    SurfaceData surface,
    float3 integrated_diffuse,
    float3 integrated_specular,
    float2 integrated_brdf,
    uint surface_model
) {
    switch (surface_model) {
        case MATERIAL_SURFACE_MODEL_UNLIT:
            return surface_eval_lut_unlit(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf);
        case MATERIAL_SURFACE_MODEL_LIT:
            return surface_eval_lut_lit(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf);
        default:
            return float3(0.0, 0.0, 0.0);
    }
}

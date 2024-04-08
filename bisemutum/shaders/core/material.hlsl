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

void surface_eval(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface,
    out float3 diffuse_result,
    out float3 specular_result
) {
#if MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_UNLIT
    surface_eval_unlit(N, T, B, V, L, surface, diffuse_result, specular_result);
#elif MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_LIT
    surface_eval_lit(N, T, B, V, L, surface, diffuse_result, specular_result);
#else
    diffuse_result = 0.0;
    specular_result = 0.0;
#endif
}
float3 surface_eval(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface
) {
    float3 diffuse_result;
    float3 specular_result;
    surface_eval(N, T, B, V, L, surface, diffuse_result, specular_result);
    return diffuse_result + specular_result;
}

void surface_eval_lut(
    float3 N,
    float3 V,
    SurfaceData surface,
    float3 integrated_diffuse,
    float3 integrated_specular,
    float2 integrated_brdf,
    out float3 diffuse_result,
    out float3 specular_result
) {
#if MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_UNLIT
    surface_eval_lut_unlit(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf, diffuse_result, specular_result);
#elif MATERIAL_SURFACE_MODEL == MATERIAL_SURFACE_MODEL_LIT
    surface_eval_lut_lit(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf, diffuse_result, specular_result);
#else
    diffuse_result = 0.0;
    specular_result = 0.0;
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
    float3 diffuse_result;
    float3 specular_result;
    surface_eval_lut(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf, diffuse_result, specular_result);
    return diffuse_result + specular_result;
}

void surface_eval(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface,
    uint surface_model,
    out float3 diffuse_result,
    out float3 specular_result
) {
    switch (surface_model) {
        case MATERIAL_SURFACE_MODEL_UNLIT:
            surface_eval_unlit(N, T, B, V, L, surface, diffuse_result, specular_result);
            break;
        case MATERIAL_SURFACE_MODEL_LIT:
            surface_eval_lit(N, T, B, V, L, surface, diffuse_result, specular_result);
            break;
        default:
            diffuse_result = 0.0;
            specular_result = 0.0;
            break;
    }
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
    float3 diffuse_result;
    float3 specular_result;
    surface_eval(N, T, B, V, L, surface, surface_model, diffuse_result, specular_result);
    return diffuse_result + specular_result;
}

void surface_eval_lut(
    float3 N,
    float3 V,
    SurfaceData surface,
    float3 integrated_diffuse,
    float3 integrated_specular,
    float2 integrated_brdf,
    uint surface_model,
    out float3 diffuse_result,
    out float3 specular_result
) {
    switch (surface_model) {
        case MATERIAL_SURFACE_MODEL_UNLIT:
            surface_eval_lut_unlit(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf, diffuse_result, specular_result);
            break;
        case MATERIAL_SURFACE_MODEL_LIT:
            surface_eval_lut_lit(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf, diffuse_result, specular_result);
            break;
        default:
            diffuse_result = 0.0;
            specular_result = 0.0;
            break;
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
    float3 diffuse_result;
    float3 specular_result;
    surface_eval_lut(N, V, surface, integrated_diffuse, integrated_specular, integrated_brdf, surface_model, diffuse_result, specular_result);
    return diffuse_result + specular_result;
}

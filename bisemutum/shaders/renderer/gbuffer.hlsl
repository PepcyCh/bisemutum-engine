#pragma once

#include "../core/material/utils.hlsl"
#include "../core/utils/pack.hlsl"

struct GBuffer {
    float4 base_color;
    float4 normal_roughness;
    float4 specular_model;
    float4 additional_0;
};

#define GBUFFER_BASE_COLOR 0
#define GBUFFER_NORMAL_ROUGHNESS 1
#define GBUFFER_SPECULAR_MODEL 2
#define GBUFFER_ADDITIONAL_0 3

GBuffer pack_surface_to_gbuffer(float3 V, float3 N, float3 T, SurfaceData surface, uint surface_model) {
    GBuffer gbuffer;

    gbuffer.base_color = float4(surface.base_color, 1.0);
    float3 fr = schlick_fresnel(surface.f0_color, surface.f90_color, max(dot(V, N), 0.0), surface.ior);
    gbuffer.specular_model = float4(fr, surface_model);

    float3 packed_frame = pack_normal_and_tangent(N, T);
    gbuffer.normal_roughness = float4(packed_frame, surface.roughness);

    gbuffer.additional_0 = float4(surface.anisotropy, 0.0, 0.0, 1.0);

    return gbuffer;
}

void unpack_gbuffer_to_surface(GBuffer gbuffer, out float3 N, out float3 T, out SurfaceData surface, out uint surface_model) {
    surface_model = (uint) gbuffer.specular_model.w;
    surface.base_color = gbuffer.base_color.xyz;
    surface.f0_color = gbuffer.specular_model.xyz;
    surface.f90_color = gbuffer.specular_model.xyz;
    surface.roughness = gbuffer.normal_roughness.w;
    unpack_normal_and_tangent(gbuffer.normal_roughness.xyz, N, T);
    surface.ior = 1.5;
    surface.opacity = 1.0;
    surface.anisotropy = gbuffer.additional_0.x;
}

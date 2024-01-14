#pragma once

#include "../core/material/utils.hlsl"
#include "../core/utils/pack.hlsl"

struct GBuffer {
    float4 base_color;
    float4 normal_roughness;
    float4 fresnel;
    float4 material_0;
};

#define GBUFFER_BASE_COLOR 0
#define GBUFFER_NORMAL_ROUGHNESS 1
#define GBUFFER_FRESNEL 2
#define GBUFFER_MATERIAL_0 3

GBuffer pack_surface_to_gbuffer(float3 V, float3 N, float3 T, SurfaceData surface, uint surface_model) {
    GBuffer gbuffer;

    gbuffer.base_color = float4(surface.base_color, 1.0);
    gbuffer.fresnel = float4(
        pack_u32_to_unorm16x2(pack_color_rg11b10(surface.f0_color)),
        pack_u32_to_unorm16x2(pack_color_rg11b10(surface.f90_color))
    );

    float3 packed_frame = pack_normal_and_tangent(N, T);
    gbuffer.normal_roughness = float4(packed_frame, surface.roughness);

    gbuffer.material_0 = float4(surface.anisotropy, 1.0 / surface.ior, 0.0, surface_model / 256.0);

    return gbuffer;
}

void unpack_gbuffer_to_surface(GBuffer gbuffer, out float3 N, out float3 T, out SurfaceData surface, out uint surface_model) {
    surface_model = (uint) (gbuffer.material_0.w * 255.5);
    surface.base_color = gbuffer.base_color.xyz;
    surface.f0_color = unpack_color_rg11b10(unpack_u32_from_unorm16x2(gbuffer.fresnel.xy));
    surface.f90_color = unpack_color_rg11b10(unpack_u32_from_unorm16x2(gbuffer.fresnel.zw));
    surface.roughness = gbuffer.normal_roughness.w;
    unpack_normal_and_tangent(gbuffer.normal_roughness.xyz, N, T);
    surface.anisotropy = gbuffer.material_0.x;
    surface.ior = 1.0 / gbuffer.material_0.y;
    surface.opacity = 1.0;
}

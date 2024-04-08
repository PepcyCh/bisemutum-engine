#pragma once

#include "utils.hlsl"

void surface_eval_lit(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface,
    out float3 diffuse_result,
    out float3 specular_result
) {
    float3 H = normalize(V + L);
    float3 local_h = float3(dot(H, T), dot(H, B), dot(H, N));
    float3 local_v = float3(dot(V, T), dot(V, B), dot(V, N));
    float3 local_l = float3(dot(L, T), dot(L, B), dot(L, N));
    if (local_v.z <= 0.0 || local_l.z <= 0.0) {
        diffuse_result = 0.0;
        specular_result = 0.0;
        return;
    }

    float3 fr = schlick_fresnel(surface.f0_color, surface.f90_color, max(dot(V, H), 0.0), surface.ior);

    diffuse_result = (1.0 - fr) * surface.base_color * INV_PI * max(local_l.z, 0.0);

    float roughness_x, roughness_y;
    get_anisotropic_roughness(surface.roughness, surface.anisotropy, roughness_x, roughness_y);

    float ndf = ggx_ndf(local_h, roughness_x, roughness_y);
    float vis = ggx_visible_hc(local_v, local_l, roughness_x, roughness_y);
    specular_result = fr * ndf * vis * max(local_l.z, 0.0);
}

void surface_eval_lut_lit(
    float3 N,
    float3 V,
    SurfaceData surface,
    float3 integrated_diffuse,
    float3 integrated_specular,
    float2 integrated_brdf,
    out float3 diffuse_result,
    out float3 specular_result
) {
    float ndotv = dot(N, V);
    if (ndotv <= 0.0) {
        diffuse_result = 0.0;
        specular_result = 0.0;
        return;
    }
    float3 fr = schlick_fresnel(surface.f0_color, surface.f90_color, ndotv, surface.ior);
    float3 diffuse = (1.0 - fr) * surface.base_color * INV_PI;
    float3 specular = surface.f0_color * integrated_brdf.x + surface.f90_color * integrated_brdf.y;
    diffuse_result = diffuse * integrated_diffuse;
    specular_result = specular * integrated_specular;
}

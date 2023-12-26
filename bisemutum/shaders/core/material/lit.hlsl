#pragma once

#include "utils.hlsl"

float3 surface_eval_lit(
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    float3 L,
    SurfaceData surface
) {
    float3 H = normalize(V + L);
    float3 local_h = float3(dot(H, T), dot(H, B), dot(H, N));
    float3 local_v = float3(dot(V, T), dot(V, B), dot(V, N));
    float3 local_l = float3(dot(L, T), dot(L, B), dot(L, N));
    if (local_v.z <= 0.0 || local_l.z <= 0.0) { return 0.0; }

    float3 fr = schlick_fresnel(surface.f0_color, surface.f90_color, max(dot(V, H), 0.0), surface.ior);

    float3 diffuse = (1.0 - fr) * surface.base_color * INV_PI;

    float roughness_x, roughness_y;
    get_anisotropic_roughness(surface.roughness, surface.anisotropy, roughness_x, roughness_y);

    float ndf = ggx_ndf(local_h, roughness_x, roughness_y);
    float vis = ggx_visible_hc(local_v, local_l, roughness_x, roughness_y);
    float3 specular = fr * ndf * vis;

    return (diffuse + specular) * max(local_l.z, 0.0);
}

float3 surface_eval_ibl_lit(
    float3 N,
    float3 V,
    SurfaceData surface,
    float3 ibl_diffuse,
    float3 ibl_specular,
    float2 ibl_brdf
) {
    float ndotv = dot(N, V);
    if (ndotv <= 0.0) { return 0.0; }
    float3 fr = schlick_fresnel(surface.f0_color, surface.f90_color, ndotv, surface.ior);
    float3 diffuse = (1.0 - fr) * surface.base_color * INV_PI;
    float3 specular = surface.f0_color * ibl_brdf.x + surface.f90_color * ibl_brdf.y;
    return diffuse * ibl_diffuse + specular * ibl_specular;
}

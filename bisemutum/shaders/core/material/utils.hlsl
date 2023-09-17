#pragma once

#include "../math.hlsl"

struct SurfaceData {
    float3 base_color;
    float3 f0_color;
    float3 f90_color;
    float3 normal_map_value;
    float roughness;
    float anisotropy;
    float ior;
};

float3 schlick_mix(float3 f0, float3 f90, float cos_theta) {
    return lerp(f0, f90, pow5(1.0 - cos_theta));
}

float3 schlick_fresnel(float3 f0, float3 f90, float cos_theta, float ior) {
    if (cos_theta < 0.0) {
        float eta = 1.0 / ior;
        float sin_theta_sqr = eta * eta * (1.0 - cos_theta * cos_theta);
        cos_theta = sqrt(max(1.0 - sin_theta_sqr, 0.0));
    }
    return schlick_mix(f0, f90, cos_theta);
}


void get_anisotropic_roughness(float roughness, float anisotropy, out float roughness_x, out float roughness_y) {
    float aniso = sqrt(1.0 - anisotropy * 0.9);
    float r2 = roughness * roughness;
    roughness_x = max(r2 / aniso, 0.001);
    roughness_y = max(r2 * aniso, 0.001);
}


float ggx_ndf(float3 local_h, float roughness_x, float roughness_y) {
    float a = Pow2(local_h.x / roughness_x) + Pow2(local_h.y / roughness_y) + Pow2(local_h.z);
    return INV_PI / (roughness_x * roughness_y * a * a);
}

float ggx_separable_visible(float3 local_v, float3 local_l, float roughness_x, float roughness_y) {
    float v = local_v.z + sqrt(Pow2(roughness_x * local_v.x) + Pow2(roughness_y * local_v.y) + Pow2(local_v.z));
    float l = local_l.z + sqrt(Pow2(roughness_x * local_l.x) + Pow2(roughness_y * local_l.y) + Pow2(local_l.z));
    return 1.0 / max(v * l, 0.0001);
}

#pragma once

#include "../utils/math.hlsl"

struct SurfaceData {
    float3 emission;
    float3 base_color;
    float3 f0_color;
    float3 f90_color;
    float3 normal_map_value;
    float roughness;
    float anisotropy;
    float ior;
    float opacity;
    bool two_sided;
};

SurfaceData surface_data_default() {
    SurfaceData surface;
    surface.emission = 0.0;
    surface.base_color = 0.5;
    surface.f0_color = 0.04;
    surface.f90_color = 1.0;
    surface.normal_map_value = float3(0.5, 0.5, 1.0);
    surface.roughness = 0.5;
    surface.anisotropy = 0.0;
    surface.ior = 1.5;
    surface.opacity = 1.0;
    surface.two_sided = false;
    return surface;
}
SurfaceData surface_data_diffuse(float3 base_color) {
    SurfaceData surface = surface_data_default();
    surface.base_color = base_color;
    return surface;
}

#define ALPHA_TEST_OPACITY_THRESHOLD 0.01

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
    float a = pow2(local_h.x / roughness_x) + pow2(local_h.y / roughness_y) + pow2(local_h.z);
    return INV_PI / (roughness_x * roughness_y * a * a);
}

float ggx_g1(float3 local_v, float roughness_x, float roughness_y) {
    float a = (pow2(roughness_x * local_v.x) + pow2(roughness_y * local_v.y)) / max(local_v.z * local_v.z, 0.0001);
    return 2.0 / (1.0 + sqrt(1.0 + a));
}

// 'sep' for separable
float ggx_g2_sep(float3 local_v, float3 local_l, float roughness_x, float roughness_y) {
    return ggx_g1(local_v, roughness_x, roughness_y) * ggx_g1(local_l, roughness_x, roughness_y);
}

// 'sep' for separable
float ggx_visible_sep(float3 local_v, float3 local_l, float roughness_x, float roughness_y) {
    float v = local_v.z + sqrt(pow2(roughness_x * local_v.x) + pow2(roughness_y * local_v.y) + pow2(local_v.z));
    float l = local_l.z + sqrt(pow2(roughness_x * local_l.x) + pow2(roughness_y * local_l.y) + pow2(local_l.z));
    return 1.0 / max(v * l, 0.0001);
}

// 'hc' for height correlated
float ggx_g2_hc(float3 local_v, float3 local_l, float roughness_x, float roughness_y) {
    float v = local_l.z * sqrt(pow2(roughness_x * local_v.x) + pow2(roughness_y * local_v.y) + pow2(local_v.z));
    float l = local_v.z * sqrt(pow2(roughness_x * local_l.x) + pow2(roughness_y * local_l.y) + pow2(local_l.z));
    return 2.0 * local_v.z * local_l.z / max(v + l, 0.0001);
}

// 'hc' for height correlated
float ggx_visible_hc(float3 local_v, float3 local_l, float roughness_x, float roughness_y) {
    float v = local_l.z * sqrt(pow2(roughness_x * local_v.x) + pow2(roughness_y * local_v.y) + pow2(local_v.z));
    float l = local_v.z * sqrt(pow2(roughness_x * local_l.x) + pow2(roughness_y * local_l.y) + pow2(local_l.z));
    return 0.5 / max(v + l, 0.0001);
}

float3 ggx_vndf_sample(float3 local_v, float roughness_x, float roughness_y, float2 rand) {
    if (local_v.z < 0.0) { local_v = -local_v; }

    float3 vh = normalize(float3(roughness_x * local_v.x, roughness_y * local_v.y, local_v.z));
    float len_sqr = vh.x * vh.x + vh.y * vh.y;
    float3 t_vec1 = len_sqr > 0.0 ? float3(-vh.y, vh.x, 0.0) / sqrt(len_sqr) : float3(1.0, 0.0, 0.0);
    float3 t_vec2 = cross(vh, t_vec1);

    float r = sqrt(rand.x);
    float phi = TWO_PI * rand.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    float3 nh = t1 * t_vec1 + t2 * t_vec2 + sqrt(max(1.0 - t1 * t1 - t2 * t2, 0.0)) * vh;
    float3 h = normalize(float3(roughness_x * nh.x, roughness_y * nh.y, max(nh.z, 0.0)));

    return h;
}

float ggx_vndf_sample_pdf(float3 h, float3 v, float roughness_x, float roughness_y) {
    return ggx_g1(v, roughness_x, roughness_y) * ggx_ndf(h, roughness_x, roughness_y) * max(dot(h, v), 0.0)
        / max(v.z, 0.0001);
}

float ggx_vndf_sample_weight_sep(float3 v, float3 l, float roughness_x, float roughness_y) {
    return ggx_g1(l, roughness_x, roughness_y);
}

float ggx_vndf_sample_weight_hc(float3 v, float3 l, float roughness_x, float roughness_y) {
    return ggx_g2_hc(v, l, roughness_x, roughness_y) / ggx_g1(v, roughness_x, roughness_y);
}

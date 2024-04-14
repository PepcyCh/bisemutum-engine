#pragma once

#include <bisemutum/shaders/core/utils/frame.hlsl>
#include <bisemutum/shaders/core/utils/math.hlsl>

#define REBLUR_TILE_SIZE 8

#define REBLUR_NUM_MAX_ACCUM_FRAME 32.0
#define REBLUR_NUM_MIP_LEVEL 4.0
#define REBLUR_NUM_MAX_FRAME_WITH_HISTORY_FIX 4.0

float2 rotate_vector(float4 rotator, float2 v) {
    return v.x * rotator.xy + v.y * rotator.zw;
}

float get_specular_lobe_half_angle(float roughness, float percentage) {
    float m = roughness * roughness;
    return atan(m * percentage / (1.0 - percentage));
}

float get_specular_magic_curve2(float roughness, float percentage) {
    float angle = get_specular_lobe_half_angle(roughness, percentage);
    float almost_half_pi = get_specular_lobe_half_angle(1.0, percentage);
    return saturate(angle / almost_half_pi);
}

float get_specular_dominant_factor(float ndotv, float roughness) {
    float a = 0.298475 * log(39.4115 - 39.0029 * roughness);
    float dominant_factor = pow(saturate(1.0 - ndotv), 10.8649) * (1.0 - a) + a;
    return saturate(dominant_factor);
}

float3 get_specular_dominant_direction_with_factor(float3 N, float3 V, float dominant_factor) {
    float3 R = reflect(-V, N);
    float3 D = lerp(N, R, dominant_factor);
    return normalize(D);
}

float4 get_specular_dominant_direction(float3 N, float3 V, float roughness) {
    float ndotv = abs(dot(N, V));
    float dominant_factor = get_specular_dominant_factor(ndotv, roughness);
    return float4(get_specular_dominant_direction_with_factor(N, V, dominant_factor), dominant_factor);
}

float2x3 get_kernel_basis(float3 V, float3 N, float roughness) {
    Frame basis = create_frame(N);
    float3 T = basis.x;
    float3 B = basis.y;
    float ndotv = abs(dot(N, V));
    float3 D = get_specular_dominant_direction(N, V, roughness).xyz;
    float ndotd = abs(dot(N, D));

    if (ndotd < 0.999 && roughness != 1.0) {
        float3 d_reflected = reflect(-D, N);
        T = normalize(cross(N, d_reflected));
        B = cross(d_reflected, T);

        float ndotv = abs(dot(N, V));
        float acos01sq = saturate(1.0 - ndotv);
        float skew_factor = lerp(1.0, roughness, sqrt(acos01sq));
        T *= skew_factor;
    }

    return float2x3(T, B);
}

float calc_parallax(float3 curr_view, float3 prev_view) {
    float cosa = saturate(dot(curr_view, prev_view));
    return sqrt(1.0 - cosa * cosa) / max(cosa, 0.00001) * 60;
}

// SPEC_ACCUM_CURVE = 1.0 (aggressiveness of history rejection depending on viewing angle: 1 = low, 0.66 = medium, 0.5 = high)
#define SPEC_ACCUM_CURVE 0.5
// SPEC_ACCUM_BASE_POWER = 0.5-1.0 (greater values lead to less aggressive accumulation)
#define SPEC_ACCUM_BASE_POWER 1.0
float get_specular_accum_speed(float roughness, float ndotv, float parallax) {
    float acos01sq = 1.0 - ndotv;
    float a = pow(saturate(acos01sq), SPEC_ACCUM_CURVE);
    float b = 1.1 + roughness * roughness;
    float parallax_sensitivity = (b + a) / (b - a);
    float power_scale = 1.0 + parallax * parallax_sensitivity;
    float f = 1.0 - exp2(-200.0 * roughness * roughness);
    f *= pow(saturate(roughness), SPEC_ACCUM_BASE_POWER * power_scale);
    return REBLUR_NUM_MAX_ACCUM_FRAME * f;
}

float hit_distance_attenuation(float roughness, float camera_dist, float hit_dist) {
    float f = hit_dist < 0.0 ? 1.0 : hit_dist / (hit_dist + camera_dist);
    return lerp(0.5 * roughness, 1.0, f);
}

float3 get_virtual_position(float3 position, float3 view, float ndotv, float roughness, float hit_dist) {
    float f = get_specular_dominant_factor(ndotv, roughness);
    return position - view * hit_dist * f;
}

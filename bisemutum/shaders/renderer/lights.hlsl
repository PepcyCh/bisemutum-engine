#pragma once

#include "../core/math.hlsl"
#include "lights_struct.hlsl"

// ---- Dir Light & Point (Spot) Light ----

float3 dir_light_eval(DirLightData light, float3 pos, out float3 light_dir) {
    light_dir = light.direction;
    return light.emission;
}

float3 point_light_eval(PointLightData light, float3 position, out float3 light_dir) {
    float3 light_vec = light.position - position;
    float light_dist_sqr = dot(light_vec, light_vec);
    light_dir = light_vec / sqrt(light_dist_sqr);
    float attenuation = saturate(1.0 - pow2(light_dist_sqr * light.range_sqr_inv)) / max(light_dist_sqr, 0.001);
    if (light.cos_inner > light.cos_outer) {
        float cos_theta = clamp(dot(light_dir, light.direction), light.cos_outer, light.cos_inner);
        attenuation *= (cos_theta - light.cos_outer) / max(light.cos_inner - light.cos_outer, 0.001);
    }
    return light.emission * attenuation;
}

float test_shadow_pcf9(
    float3 position, float3 normal,
    float4x4 light_transform,
    float3 light_direction,
    float shadow_depth_bias,
    float shadow_normal_bias,
    float shadow_strength,
    Texture2DArray shadow_map, int shadow_map_index,
    SamplerState shadow_map_sampler
) {
    float cos_theta = abs(dot(normal, light_direction));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    float tan_theta = min(sin_theta / max(cos_theta, 0.001), 4.0);
    shadow_depth_bias = max(shadow_depth_bias * tan_theta, 0.00001);
    shadow_normal_bias = max(shadow_normal_bias * sin_theta, 0.001);
    float4 light_pos = mul(light_transform, float4(position + normal * shadow_normal_bias, 1.0));
    light_pos.xyz /= light_pos.w;
    float2 sm_uv = float2((light_pos.x + 1.0) * 0.5, (1.0 - light_pos.y) * 0.5);
    const float delta = 0.0002;
    float2 uv_offset[9] = {
        float2(-delta, -delta),
        float2(0.0, -delta),
        float2(delta, -delta),
        float2(-delta, 0.0),
        float2(0.0, 0.0),
        float2(delta, 0.0),
        float2(-delta, delta),
        float2(0.0, delta),
        float2(delta, delta),
    };
    float shadow_factor = 0.0;
    [unroll]
    for (int i = 0; i < 9; i++) {
        float sm_depth = shadow_map.Sample(
            shadow_map_sampler, float3(sm_uv + uv_offset[i], shadow_map_index)
        ).x;
        shadow_factor += (light_pos.z - shadow_depth_bias) < sm_depth ? 1.0 : (1.0 - shadow_strength);
    }
    shadow_factor /= 9.0;
    return shadow_factor;
}

float dir_light_shadow_factor(
    DirLightData light, float3 position, float3 normal,
    float3 camera_position,
    StructuredBuffer<float4x4> dir_lights_shadow_transform,
    Texture2DArray dir_lights_shadow_map,
    SamplerState shadow_map_sampler
) {
    float shadow_factor = 1.0;
    if (light.sm_index >= 0) {
        float3 camera_vec = camera_position - position;
        float camera_dist_sqr = dot(camera_vec, camera_vec);
        int level = -1;
        for (int i = 0; i < 4; i++) {
            if (camera_dist_sqr <= light.cascade_shadow_radius_sqr[i]) {
                level = i;
                break;
            }
        }
        if (level == -1) {
            return shadow_factor;
        }

        float4x4 light_transform = dir_lights_shadow_transform[light.sm_index + level];
        shadow_factor = test_shadow_pcf9(
            position, normal,
            light_transform,
            light.direction,
            light.shadow_depth_bias,
            light.shadow_normal_bias,
            light.shadow_strength,
            dir_lights_shadow_map, light.sm_index + level,
            shadow_map_sampler
        );
    }
    return shadow_factor;
}

float point_light_shadow_factor(
    PointLightData light, float3 position, float3 normal,
    float3 camera_position,
    StructuredBuffer<float4x4> point_lights_shadow_transform,
    Texture2DArray point_lights_shadow_map,
    SamplerState shadow_map_sampler
) {
    float shadow_factor = 1.0;
    if (light.sm_index >= 0) {
        float3 light_dir = position - light.position;
        if (light.cos_inner > light.cos_outer) {
            float4x4 light_transform = point_lights_shadow_transform[light.sm_index];
            shadow_factor = test_shadow_pcf9(
                position, normal,
                light_transform,
                light.direction,
                light.shadow_depth_bias,
                light.shadow_normal_bias * abs(dot(light_dir, light.direction)),
                light.shadow_strength,
                point_lights_shadow_map, light.sm_index,
                shadow_map_sampler
            );
        } else {
            float dir_abs_x = abs(light_dir.x);
            float dir_abs_y = abs(light_dir.y);
            float dir_abs_z = abs(light_dir.z);
            int face;
            float3 sm_light_dir;
            if (dir_abs_x > dir_abs_y && dir_abs_x > dir_abs_z) {
                face = light_dir.x > 0.0 ? 0 : 1;
                // Since `abs()` is used when using `sm_light_dir`, the sign doesn't matter
                sm_light_dir = float3(1.0, 0.0, 0.0);
            } else if (dir_abs_y > dir_abs_z) {
                face = light_dir.y > 0.0 ? 2 : 3;
                sm_light_dir = float3(0.0, 1.0, 0.0);
            } else {
                face = light_dir.z > 0.0 ? 4 : 5;
                sm_light_dir = float3(0.0, 0.0, 1.0);
            }
            float4x4 light_transform = point_lights_shadow_transform[light.sm_index + face];
            shadow_factor = test_shadow_pcf9(
                position, normal,
                light_transform,
                sm_light_dir,
                light.shadow_depth_bias,
                light.shadow_normal_bias * abs(dot(light_dir, sm_light_dir)),
                light.shadow_strength,
                point_lights_shadow_map, light.sm_index + face,
                shadow_map_sampler
            );
        }
    }
    return shadow_factor;
}

// ---- Rect Light ----
// Most of the codes are modified from https://github.com/AakashKT/LTC-Anisotropic

void get_ltc_tex3d_coord(float4 u, out float3 u1, out float3 u2, out float w) {
    float ws = u.w * 7.0;
    float ws_f = floor(ws);
    float ws_c = min(floor(ws + 1.0), 7.0);
    w = fract(ws);

    float x = (u.x * 7.0 + 0.5) / 8.0;
    float y = (u.y * 7.0 + 0.5) / 8.0;
    float z1 = ((u.z * 7.0 + 8.0 * ws_f + 0.5) / 64.0);
    float z2 = ((u.z * 7.0 + 8.0 * ws_c + 0.5) / 64.0);

    u1 = float3(x, y, z1);
    u2 = float3(x, y, z2);
}
float3x3 fetch_fetch_ltc_matrix(float3 u) {
    float3 m0 = ltc_matrix_lut0.Sample(ltc_sampler, u).xyz;
    float3 m1 = ltc_matrix_lut1.Sample(ltc_sampler, u).xyz;
    float3 m2 = ltc_matrix_lut2.Sample(ltc_sampler, u).xyz;
    return transpose(float3x3(m0, m1, m2));
}
float3x3 fetch_ltc_matrix(float4 u) {
    float3 u1, u2;
    float w;
    get_ltc_tex3d_coord(u, u1, u2, w);

    float3x3 m1 = fetch_fetch_ltc_matrix(u1);
    float3x3 m2 = fetch_fetch_ltc_matrix(u2);
    return lerp(m1, m2, w);
}
float fetch_ltc_norm(float4 u) {
    float3 u1, u2;
    float w;
    get_ltc_tex3d_coord(u, u1, u2, w);

    float n1 = ltc_norm_lut.Sample(ltc_sampler, u1).x;
    float n2 = ltc_norm_lut.Sample(ltc_sampler, u2).x;
    return lerp(n1, n2, w);
}

void get_ltc_matrix_and_norm(
    float3 local_v, float roughness_x, float roughness_y,
    inout float4x3 L, out float3x3 ltc_matrix, out float ltc_norm
) {
    float theta_wi = acos(local_v.z);
    bool flip_roughness = roughness_y > roughness_x;
    float phi_wi = atan(local_v.y, local_v.x);
    phi_wi = flip_roughness ? (PI / 2.0 - phi_wi) : phi_wi;
    phi_wi = phi_wi >= 0.0 ? phi_wi : phi_wi + 2.0 * PI;
    float u0 = ((flip_roughness ? roughness_y : roughness_x) - 0.001) / (1.0 - 0.001);
    float u1 = (flip_roughness ? roughness_x / roughness_y : roughness_y / roughness_x);
    float u2 = theta_wi / PI * 2.0;

    if (phi_wi < PI * 0.5) {
        float u3 = phi_wi / (PI * 0.5);
        float4 u = float4(u3, u2, u1, u0);
        float4x4 winding = transpose(float4x4(
            0, 0, 0, 1,
            0, 0, 1, 0,
            0, 1, 0, 0,
            1, 0, 0, 0
        ));

        L = transpose(mul(winding, transpose(L)));
        ltc_matrix = fetch_ltc_matrix(u);
        ltc_norm = fetch_ltc_norm(u);
    } else if (phi_wi >= PI * 0.5 && phi_wi < PI) {
        float u3 = (PI - phi_wi) / (PI * 0.5);
        float4 u = float4(u3, u2, u1, u0);
        float3x3 flip = float3x3(
            -1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0
        );
        ltc_matrix = mul(flip, fetch_ltc_matrix(u));
        ltc_norm = fetch_ltc_norm(u);
    } else if (phi_wi >= PI && phi_wi < 1.5 * PI) {
        float u3 = (phi_wi - PI) / (PI * 0.5);
        float4 u = float4(u3, u2, u1, u0);
        float3x3 flip = float3x3(
            -1.0, 0.0, 0.0,
            0.0, -1.0, 0.0,
            0.0, 0.0, 1.0
        );
        float4x4 winding = transpose(float4x4(
            0, 0, 0, 1,
            0, 0, 1, 0,
            0, 1, 0, 0,
            1, 0, 0, 0
        ));

        L = transpose(mul(winding, transpose(L)));
        ltc_matrix = mul(flip, fetch_ltc_matrix(u));
        ltc_norm = fetch_ltc_norm(u);
    } else if (phi_wi >= 1.5 * PI && phi_wi < 2.0 * PI) {
        float u3 = (2.0 * PI - phi_wi) / (PI * 0.5);
        float4 u = vec4(u3, u2, u1, u0);
        float3x3 flip = float3x3(
            1.0, 0.0, 0.0,
            0.0, -1.0, 0.0,
            0.0, 0.0, 1.0
        );

        ltc_matrix = mul(flip, fetch_ltc_matrix(u));
        ltc_norm = fetch_ltc_norm(u);
    }
    if (flip_roughness) {
        float3x3 rotate = float3x3(
            0, 1, 0,
            1, 0, 0,
            0, 0, 1
        );
        float4x4 winding = transpose(float4x4(
            0, 0, 0, 1,
            0, 0, 1, 0,
            0, 1, 0, 0,
            1, 0, 0, 0
        ));

        L = transpose(mul(winding, transpose(L)));
        ltc_matrix = mul(rotate, ltc_matrix);
    }

    ltc_matrix = inverse(ltc_matrix);
}

void ltc_clip_quad(inout float3 L[5], out int n) {
    // detect clipping config
    int config = 0;
    if (L[0].z > 0.0) { config += 1 };
    if (L[1].z > 0.0) { config += 2 };
    if (L[2].z > 0.0) { config += 4 };
    if (L[3].z > 0.0) { config += 8 };

    // clip
    n = 0;

    if (config == 0) {
        // clip all
    } else if (config == 1) {
        // V1 clip V2 V3 V4 
        n = 3;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[3].z * L[0] + L[0].z * L[3];
    } else if (config == 2) {
        // V2 clip V1 V3 V4
        n = 3;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    } else if (config == 3) {
        // V1 V2 clip V3 V4 
        n = 4;
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
        L[3] = -L[3].z * L[0] + L[0].z * L[3];
    } else if (config == 4) {
        // V3 clip V1 V2 V4 
        n = 3;
        L[0] = -L[3].z * L[2] + L[2].z * L[3];
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
    } else if (config == 5) {
        // V1 V3 clip V2 V4) impossible
        n = 0;
    } else if (config == 6) {
        // V2 V3 clip V1 V4
        n = 4;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    } else if (config == 7) {
        // V1 V2 V3 clip V4
        n = 5;
        L[4] = -L[3].z * L[0] + L[0].z * L[3];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    } else if (config == 8) {
        // V4 clip V1 V2 V3
        n = 3;
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
        L[1] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] =  L[3];
    } else if (config == 9) {
        // V1 V4 clip V2 V3
        n = 4;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[2].z * L[3] + L[3].z * L[2];
    } else if (config == 10) {
        // V2 V4 clip V1 V3) impossible
        n = 0;
    } else if (config == 11) {
        // V1 V2 V4 clip V3
        n = 5;
        L[4] = L[3];
        L[3] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    } else if (config == 12) {
        // V3 V4 clip V1 V2
        n = 4;
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
    } else if (config == 13) {
        // V1 V3 V4 clip V2
        n = 5;
        L[4] = L[3];
        L[3] = L[2];
        L[2] = -L[1].z * L[2] + L[2].z * L[1];
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
    } else if (config == 14) {
        // V2 V3 V4 clip V1
        n = 5;
        L[4] = -L[0].z * L[3] + L[3].z * L[0];
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
    } else if (config == 15) {
        // V1 V2 V3 V4
        n = 4;
    }

    if (n == 3) { L[3] = L[0]; }
    if (n == 4) { L[4] = L[0]; }
}
float ltc_integrate_edge(float3 v1, float3 v2) {
    // float cos_theta = dot(v1, v2);
    // float theta = acos(cos_theta);
    // float res = cross(v1, v2).z * ((theta > 0.001) ? theta / sin(theta) : 1.0);

    float x = dot(v1, v2);
    float y = abs(x);
    float a = 5.42031 + (3.12829 + 0.0902326 * y) * y;
    float b = 3.45068 + (4.18814 + y) * y;
    float theta_div_sin_theta = a / b;
    if (x < 0.0) {
        theta_div_sin_theta = PI * rsqrt(1.0 - x * x) - theta_div_sin_theta;
    }
    float res = cross(v1, v2).z * theta_div_sin_theta;

    return res;
}
float ltc_integrate(
    float3 P,
    float3 N,
    float3 T,
    float3 B,
    float3x3 ltc_matrix,
    float4x3 L,
    bool two_sided
) {
    float3x3 TBN_inv = transpose(float3x3(T, B, N));

    float3 LP[5];
    LP[0] = mul(ltc_matrix, mul(TBN_inv, (L[0] - P)));
    LP[1] = mul(ltc_matrix, mul(TBN_inv, (L[1] - P)));
    LP[2] = mul(ltc_matrix, mul(TBN_inv, (L[2] - P)));
    LP[3] = mul(ltc_matrix, mul(TBN_inv, (L[3] - P)));

    int n;
    ltc_clip_quad(LP, n);
    if (n == 0) { return 0.0; }

    // project onto sphere
    LP[0] = normalize(LP[0]);
    LP[1] = normalize(LP[1]);
    LP[2] = normalize(LP[2]);
    LP[3] = normalize(LP[3]);
    LP[4] = normalize(LP[4]);

    // integrate
    float sum = 0.0;
    sum += ltc_integrate_edge(LP[0], LP[1]);
    sum += ltc_integrate_edge(LP[1], LP[2]);
    sum += ltc_integrate_edge(LP[2], LP[3]);
    if (n >= 4) { sum += ltc_integrate_edge(LP[3], LP[4]); }
    if (n == 5) { sum += ltc_integrate_edge(LP[4], LP[0]); }

    sum = two_sided ? abs(sum) : max(0.0, sum);

    return sum;
}

float3 rect_light_eval_ltc(
    float3 P,
    float3 N,
    float3 T,
    float3 B,
    float3 V,
    RectLightData light,
    float roughness_x, float roughness_y, float3 f0_color, float3 f90_color
) {
    float4x3 L = {
        light.position0,
        light.position1,
        light.position2,
        light.position3,
    };

    float3 local_v = float3(dot(V, T), dot(V, B), dot(V, N));
    float3x3 ltc_matrix;
    float ltc_norm;
    get_ltc_matrix_and_norm(local_v, roughness_x, roughness_y, L, ltc_matrix, ltc_norm);

    float ltc_integral = ltc_integrate(P, N, T, B, ltc_matrix, L, light.two_sided);
    return ltc_integral * ltc_norm;
}

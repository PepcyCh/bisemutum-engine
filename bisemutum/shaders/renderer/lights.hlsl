#pragma once

#include "../core/math.hlsl"
#include "lights_struct.hlsl"

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
    const float delta = 0.001;
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

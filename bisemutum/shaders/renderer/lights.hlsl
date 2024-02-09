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

        float4 light_pos = mul(light_transform, float4(position, 1.0));
        light_pos.xyz /= light_pos.w;
        float2 sm_uv = float2((light_pos.x + 1.0) * 0.5, (1.0 - light_pos.y) * 0.5);
        float cos_theta = abs(dot(normal, light.direction));
        float tan_theta = sqrt(1.0 - cos_theta * cos_theta) / max(cos_theta, 0.001);
        float delta = 0.001;
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
        shadow_factor = 0.0;
        [unroll]
        for (int i = 0; i < 9; i++) {
            float sm_depth = dir_lights_shadow_map.Sample(
                shadow_map_sampler, float3(sm_uv + uv_offset[i], light.sm_index + level)
            ).x;
            shadow_factor += (light_pos.z - light.shadow_bias_factor * tan_theta) < sm_depth
                ? 1.0 : (1.0 - light.shadow_strength);
        }
        shadow_factor /= 9.0;
    }
    return shadow_factor;
}

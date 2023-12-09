#pragma once

struct LightData {
    float3 emission;
    float range_sqr;
    float3 position;
    float cos_inner;
    float3 direction;
    float cos_outer;
    int sm_index;
    float shadow_strength;

    float2 _pad1;
};

float3 dir_light_eval(LightData light, float3 pos, out float3 light_dir) {
    light_dir = light.direction;
    return light.emission;
}

float3 point_light_eval(LightData light, float3 position, out float3 light_dir) {
    float3 light_vec = light.position - position;
    float light_dist_sqr = dot(light_vec, light_vec);
    light_dir = light_vec / sqrt(light_dist_sqr);
    float attenuation = light_dist_sqr < light.range_sqr ? 1.0 / light_dist_sqr : 0.0;
    return light.emission * attenuation;
}

float3 spot_light_eval(LightData light, float3 position, out float3 light_dir) {
    float3 light_vec = light.position - position;
    float light_dist_sqr = dot(light_vec, light_vec);
    light_dir = light_vec / sqrt(light_dist_sqr);
    float cos_theta = clamp(dot(light_dir, light.direction), light.cos_outer, light.cos_inner);
    float dist_attenuation = light_dist_sqr < light.range_sqr ? 1.0 / light_dist_sqr : 0.0;
    float angle_attenuation = (cos_theta - light.cos_outer) / max(light.cos_inner - light.cos_outer, 0.001);
    return light.emission * angle_attenuation * dist_attenuation;
}

float dir_light_shadow_factor(
    LightData light, float3 position, float4x4 light_transform,
    Texture2DArray shadow_map_atlas, SamplerState shadow_map_atlas_sampler
) {
    float shadow_factor = 1.0;
    if (light.sm_index >= 0) {
        float4 light_pos = mul(light_transform, float4(position, 1.0));
        light_pos.xyz /= light_pos.w;
        float2 sm_uv = float2((light_pos.x + 1.0) * 0.5, (1.0 - light_pos.y) * 0.5);
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
            float sm_depth = shadow_map_atlas.Sample(
                shadow_map_atlas_sampler, float3(sm_uv + uv_offset[i], light.sm_index)
            ).x;
            shadow_factor += light_pos.z - 0.003 < sm_depth ? 1.0 : (1.0 - light.shadow_strength);
        }
        shadow_factor /= 9.0;
    }
    return shadow_factor;
}

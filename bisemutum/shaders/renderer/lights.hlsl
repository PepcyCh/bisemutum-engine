#pragma once

struct LightData {
    float3 emission;
    float range_sqr;
    float3 position;
    float cos_inner;
    float3 direction;
    float cos_outer;
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

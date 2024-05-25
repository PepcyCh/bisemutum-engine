#pragma once

#include <bisemutum/shaders/core/utils/random.hlsl>

#include "../../gbuffer.hlsl"

struct GBufferPayload {
    GBuffer gbuffer;
    float3 history_position;
    float hit_t;
    float opacity_random;
};

float gen_opacity_random_for(float3 ray_origin, float3 ray_direction, uint seed) {
    uint random_seed = asuint(ray_origin.x) ^ asuint(ray_origin.y) ^ asuint(ray_origin.z)
        ^ asuint(ray_direction.x) ^ asuint(ray_direction.y) ^ asuint(ray_direction.z);
    uint rng_state = rng_tea(random_seed, seed);
    return rng_next(rng_state);
}

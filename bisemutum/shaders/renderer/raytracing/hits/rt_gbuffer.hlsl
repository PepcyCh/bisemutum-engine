#pragma once

#include "../../gbuffer.hlsl"

struct GBufferPayload {
    GBuffer gbuffer;
    float3 history_position;
    float hit_t;
    float opacity_random;
};

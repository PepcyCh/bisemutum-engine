#pragma once

#include "../../gbuffer.hlsl"

struct GBufferPayload {
    GBuffer gbuffer;
    float hit_t;
    float3 history_position;
};

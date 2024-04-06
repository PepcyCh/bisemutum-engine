#include "rt_gbuffer.hlsl"

[shader("miss")]
void rt_gbuffer_rmiss(inout GBufferPayload payload) {
    payload.hit_t = -1.0;
}

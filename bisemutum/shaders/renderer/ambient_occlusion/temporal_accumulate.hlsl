#include <bisemutum/shaders/core/math.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>

#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#define NUM_GROUP_THREADS 16

[numthreads(NUM_GROUP_THREADS, NUM_GROUP_THREADS, 1)]
void ao_temporal_accumulate_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    if (any(global_thread_id.xy >= tex_size)) { return; }

    float2 this_uv = (global_thread_id.xy + 0.5) / tex_size;
    float2 velocity = velocity_tex.Load(int3(global_thread_id.xy, 0)).xy;
    float2 history_uv = this_uv - velocity;

    float2 this_ao = ao_tex[global_thread_id.xy];
    float2 history_ao = history_ao_tex.SampleLevel(input_sampler, this_uv, 0).xy;

    float frames = this_ao.y + history_ao.y;
    float2 ao_value = float2(
        frames == 0.0 ? 1.0 : lerp(history_ao.x, this_ao.x, min(1.0 / frames, 0.05)),
        frames
    );

    ao_tex[global_thread_id.xy] = ao_value;
}

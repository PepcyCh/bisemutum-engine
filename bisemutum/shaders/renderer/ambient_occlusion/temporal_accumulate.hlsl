#include <bisemutum/shaders/core/utils/math.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>

#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "../validate_history_flags.hlsl"

#define NUM_GROUP_THREADS 16

[numthreads(NUM_GROUP_THREADS, NUM_GROUP_THREADS, 1)]
void ao_temporal_accumulate_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    if (any(global_thread_id.xy >= tex_size)) { return; }

    float2 this_uv = (global_thread_id.xy + 0.5) / tex_size;
    float2 velocity = velocity_tex.SampleLevel(input_sampler, this_uv, 0).xy;
    float2 history_uv = this_uv - velocity;

    float2 this_ao = ao_tex[global_thread_id.xy];
    float2 history_ao = history_ao_tex.SampleLevel(input_sampler, history_uv, 0).xy;

    float2 ao_value = this_ao;
#if AO_HALF_RESOLUTION
    uint hist_flags = history_validation_tex[2 * global_thread_id.xy]
        | history_validation_tex[2 * global_thread_id.xy + uint2(1, 0)]
        | history_validation_tex[2 * global_thread_id.xy + uint2(0, 1)]
        | history_validation_tex[2 * global_thread_id.xy + uint2(1, 1)];
#else
    uint hist_flags = history_validation_tex[global_thread_id.xy];
#endif
    if (hist_flags == 0) {
        float frames = this_ao.y + history_ao.y;
        ao_value = float2(lerp(history_ao.x, this_ao.x, max(1.0 / frames, 0.05)), frames);
    }

    ao_tex[global_thread_id.xy] = ao_value;
}

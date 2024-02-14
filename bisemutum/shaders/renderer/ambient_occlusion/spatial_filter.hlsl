#include "../../core/math.hlsl"
#include "../../core/utils/pack.hlsl"

#include "../../core/shader_params/compute.hlsl"

#define NUM_GROUP_THREADS 16
#define SHARED_DATA_WIDTH (NUM_GROUP_THREADS + 2)

groupshared float2 s_ao_value[SHARED_DATA_WIDTH * SHARED_DATA_WIDTH];
groupshared float s_depth[SHARED_DATA_WIDTH * SHARED_DATA_WIDTH];
groupshared float3 s_normal[SHARED_DATA_WIDTH * SHARED_DATA_WIDTH];

uint shared_data_index_of(int2 offset) {
    return (offset.y + 1) * SHARED_DATA_WIDTH + offset.x + 1;
}

void fetch_shared_data(uint index, int2 group_start) {
    int2 offset = int2(
        (int) (index % SHARED_DATA_WIDTH) - 1,
        (int) (index / SHARED_DATA_WIDTH) - 1
    );

    int3 texel_coord = int3(group_start + offset, 0);
    s_ao_value[index] = input_ao_tex.Load(texel_coord).xy;
    s_depth[index] = depth_tex.Load(texel_coord).x;
    s_normal[index] = oct_decode(normal_roughness_tex.Load(texel_coord).xy);
}

[numthreads(NUM_GROUP_THREADS, NUM_GROUP_THREADS, 1)]
void ao_spatial_filter_cs(
    uint3 global_thread_id : SV_DispatchThreadID,
    uint3 local_thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID
) {
    uint linear_local_index = local_thread_id.y * NUM_GROUP_THREADS + local_thread_id.x;
    if (linear_local_index * 2 < SHARED_DATA_WIDTH * SHARED_DATA_WIDTH) {
        fetch_shared_data(linear_local_index * 2, group_id.xy * NUM_GROUP_THREADS);
        fetch_shared_data(linear_local_index * 2 + 1, group_id.xy * NUM_GROUP_THREADS);
    }
    GroupMemoryBarrierWithGroupSync();

    if (any(global_thread_id.xy >= tex_size)) { return; }

    uint center_shared_data_index = shared_data_index_of((int2) local_thread_id.xy);
    float2 center_ao_value = s_depth[center_shared_data_index];
    float center_depth = s_depth[center_shared_data_index];
    float3 center_normal = s_normal[center_shared_data_index];

    if (center_ao_value.y == 0.0) {
        output_ao_tex[global_thread_id.xy] = center_ao_value;
        return;
    }

    float2 result = center_ao_value;
    float weight_sum = 1.0;
    [unroll]
    for (int y = -1; y <= 1; y++) {
        [unroll]
        for (int x = -1; x <= 1; x++) {
            if (x == 0 && y == 0) { continue; }
            uint shared_data_index = shared_data_index_of((int2) local_thread_id.xy + int2(x, y));
            float2 ao_value = s_ao_value[shared_data_index];
            if (ao_value.y == 0.0) { continue; }
            float depth = s_depth[shared_data_index];
            float3 normal = s_normal[shared_data_index];
            float weight = max(dot(center_normal, normal), 0.0) * exp(-abs(depth - center_depth) / center_depth);
            result += s_ao_value[shared_data_index] * weight;
            weight_sum += weight;
        }
    }
    result /= weight_sum;

    output_ao_tex[global_thread_id.xy] = result;
}

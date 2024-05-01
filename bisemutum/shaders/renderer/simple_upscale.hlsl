#include <bisemutum/shaders/core/utils/math.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/depth.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#define NUM_GROUP_THREADS 16
#define SHARED_DATA_WIDTH (NUM_GROUP_THREADS / 2 + 2)
#define NUM_SHARED_DATA (SHARED_DATA_WIDTH * SHARED_DATA_WIDTH)

groupshared float4 s_value[NUM_SHARED_DATA];
groupshared float s_depth[NUM_SHARED_DATA];
groupshared float3 s_normal[NUM_SHARED_DATA];

uint shared_data_index_of(int2 offset) {
    return (offset.y + 1) * SHARED_DATA_WIDTH + offset.x + 1;
}
uint shared_data_index_of(uint2 local_index, int2 offset) {
    uint2 addr = (uint2) ((int2) (local_index / 2) + 1 + offset);
    return clamp(addr.y * SHARED_DATA_WIDTH + addr.x, 0u, uint(NUM_SHARED_DATA - 1));
}

void fetch_shared_data(uint index, int2 group_start) {
    int2 offset = int2(
        (int) (index % SHARED_DATA_WIDTH) - 1,
        (int) (index / SHARED_DATA_WIDTH) - 1
    );
    int2 hr_pixel_coord = clamp(group_start / 2 + offset, 0, (tex_size + 1) / 2);
    int2 fr_pixel_coord = hr_pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);

    s_value[index] = input_tex.Load(int3(hr_pixel_coord, 0));
    s_depth[index] = get_linear_01_depth(depth_tex.Load(int3(fr_pixel_coord, 0)).x, matrix_inv_proj);
    s_normal[index] = oct_decode(normal_roughness_tex.Load(int3(fr_pixel_coord, 0)).xy);
}

float gaussian(float radius, float sigma) {
    float temp = radius / sigma;
    return exp(-temp * temp);
}
float lanczos(float radius, float a) {
    if (radius == 0.0) return 1.0;
    if (radius >= a) return 0.0;
    return a * sin(PI * radius) * sin(PI * radius / a) * INV_PI * INV_PI / (radius * radius);
}

[numthreads(NUM_GROUP_THREADS, NUM_GROUP_THREADS, 1)]
void simple_upscale_cs(
    uint3 global_thread_id : SV_DispatchThreadID,
    uint3 local_thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID
) {
    uint linear_local_index = local_thread_id.y * NUM_GROUP_THREADS + local_thread_id.x;
    if (linear_local_index < NUM_SHARED_DATA) {
        fetch_shared_data(linear_local_index, group_id.xy * NUM_GROUP_THREADS);
    }
    GroupMemoryBarrierWithGroupSync();

    if (any(global_thread_id.xy >= tex_size)) { return; }

    float center_depth = get_linear_01_depth(depth_tex.Load(int3(global_thread_id.xy, 0)).x, matrix_inv_proj);
    float3 center_normal = oct_decode(normal_roughness_tex.Load(int3(global_thread_id.xy, 0)).xy);

    float2 curr_subpixel = float2(
        (global_thread_id.x & 1) != 0 ? 0.75 : 0.25,
        (global_thread_id.y & 1) != 0 ? 0.75 : 0.25
    );
    float2 in_subpixel = float2(
        (frame_index & 1) != 0 ? 0.75 : 0.25,
        (frame_index & 2) != 0 ? 0.75 : 0.25
    );

    float4 sum = 0.0;
    float sum_weight = 0.0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            float2 offset = float2(dx, dy) + in_subpixel - curr_subpixel;
            float r = sqrt(length(offset));
            float w = gaussian(r, 0.2);
            // float w = lanczos(r, 1.5);

            uint tap_shared_data_index = shared_data_index_of(local_thread_id.xy, int2(dx, dy));
            float tap_depth = s_depth[tap_shared_data_index];
            float3 tap_normal = s_normal[tap_shared_data_index];
            w *= max(dot(tap_normal, center_normal), 0.0);
            // w *= (tap_depth < 1.0 ? 1.0 : 0.0);
            w *= max(0.0, 1.0 - abs(tap_depth - center_depth));

            sum += s_value[tap_shared_data_index] * w;
            sum_weight += w;
        }
    }

    float4 result = sum_weight == 0.0 ? 0.0 : sum / sum_weight;
    output_tex[global_thread_id.xy] = result;
}

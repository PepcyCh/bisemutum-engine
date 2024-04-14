#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>
#include <bisemutum/shaders/core/utils/color.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "utils.hlsl"
#include "../validate_history_flags.hlsl"

#define NUM_SHARED_DATA_WIDTH (REBLUR_TILE_SIZE + 2)
#define NUM_SHARED_DATA (NUM_SHARED_DATA_WIDTH * NUM_SHARED_DATA_WIDTH)

groupshared float4 s_lighting_dist[NUM_SHARED_DATA];
groupshared float s_depth[NUM_SHARED_DATA];
groupshared float s_linear_depth[NUM_SHARED_DATA];

void fill_shared_data(uint index, int2 start) {
    uint index_x = index % NUM_SHARED_DATA_WIDTH;
    uint index_y = index / NUM_SHARED_DATA_WIDTH;

    int2 pixel_coord = clamp(start + int2(index_x, index_y) - 1, 0, tex_size - 1);
#if REBLUR_USE_HALF_RESOLUTION
    const int2 gbuffer_pixel_coord = pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);
#else
    const int2 gbuffer_pixel_coord = pixel_coord;
#endif

    float4 lighting_dist = in_lighting_dist_tex.Load(int3(pixel_coord, 0));
    lighting_dist.xyz = rgb_to_ycocg(lighting_dist.xyz);
    s_lighting_dist[index] = lighting_dist;

    float depth = depth_tex.Load(int3(gbuffer_pixel_coord, 0)).x;
    s_depth[index] = depth;
    s_linear_depth[index] = get_linear_01_depth(depth, matrix_inv_proj);
}

uint shared_data_index(uint2 local_index, int2 offset) {
    int2 addr = int2(local_index) + 1 + offset;
    return clamp(addr.y * NUM_SHARED_DATA_WIDTH + addr.x, 0u, uint(NUM_SHARED_DATA - 1));
}

[numthreads(REBLUR_TILE_SIZE, REBLUR_TILE_SIZE, 1)]
void temporal_stabilize_cs(
    uint3 global_thread_id : SV_DispatchThreadID,
    uint3 local_thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID
) {
    const int2 pixel_coord = int2(global_thread_id.xy);
    const float2 texel_size = 1.0 / tex_size;
    const float2 center_uv = (pixel_coord + 0.5) * texel_size;

#if REBLUR_USE_HALF_RESOLUTION
    const int2 gbuffer_pixel_coord = pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);
#else
    const int2 gbuffer_pixel_coord = pixel_coord;
#endif

    const uint local_index_linear = local_thread_id.y * REBLUR_TILE_SIZE + local_thread_id.x;
    const int2 group_index = int2(group_id.xy);
    if (local_index_linear < NUM_SHARED_DATA / 2) {
        fill_shared_data(local_index_linear * 2, group_index * REBLUR_TILE_SIZE);
        fill_shared_data(local_index_linear * 2 + 1, group_index * REBLUR_TILE_SIZE);
    }
    GroupMemoryBarrierWithGroupSync();

    if (any(pixel_coord >= tex_size)) return;

    const uint center_shared_data_index = shared_data_index(local_thread_id.xy, 0);
    float center_depth = s_depth[center_shared_data_index];
    if (is_depth_background(center_depth)) {
        out_lighting_dist_tex[pixel_coord] = float4(0.0, 0.0, 0.0, -1.0);
        return;
    }

    uint validation_mask = history_validation_tex[pixel_coord];
    if (validation_mask != 0) {
        float4 lighting_dist = s_lighting_dist[center_shared_data_index];
        lighting_dist.xyz = ycocg_to_rgb(lighting_dist.xyz);
        out_lighting_dist_tex[pixel_coord] = lighting_dist;
        return;
    }

    float roughness = normal_roughness_tex.Load(int3(gbuffer_pixel_coord, 0)).w;

    float3 position_view = position_view_from_depth(center_uv, center_depth, matrix_inv_proj);
    float camera_dist = length(position_view);
    float3 position = mul(matrix_inv_view, float4(position_view, 1.0)).xyz;

    float2 velocity = velocity_tex.Load(int3(gbuffer_pixel_coord, 0)).xy;
    float2 prev_uv = (pixel_coord + 0.5) * texel_size - velocity;
    float4 prev_lighting_dist = hist_lighting_dist_tex.SampleLevel(input_sampler, prev_uv, 0);
    prev_lighting_dist.xyz = rgb_to_ycocg(prev_lighting_dist.xyz);

    float4 neighbors[9];
    neighbors[0] = s_lighting_dist[shared_data_index(local_thread_id.xy, int2(-1, -1))];
    neighbors[1] = s_lighting_dist[shared_data_index(local_thread_id.xy, int2(0, -1))];
    neighbors[2] = s_lighting_dist[shared_data_index(local_thread_id.xy, int2(1, -1))];
    neighbors[3] = s_lighting_dist[shared_data_index(local_thread_id.xy, int2(-1, 0))];
    neighbors[4] = s_lighting_dist[center_shared_data_index];
    neighbors[5] = s_lighting_dist[shared_data_index(local_thread_id.xy, int2(1, 0))];
    neighbors[6] = s_lighting_dist[shared_data_index(local_thread_id.xy, int2(-1, 1))];
    neighbors[7] = s_lighting_dist[shared_data_index(local_thread_id.xy, int2(0, 1))];
    neighbors[8] = s_lighting_dist[shared_data_index(local_thread_id.xy, int2(1, 1))];

    float center_linear_depth = s_linear_depth[center_shared_data_index];
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            uint neigh_shared_data_index = shared_data_index(local_thread_id.xy, int2(dx, dy));
            if (
                is_depth_background(s_depth[neigh_shared_data_index])
                || abs(s_linear_depth[neigh_shared_data_index] - center_linear_depth) > center_linear_depth * 0.2
            ) {
                neighbors[(dy + 1) * 3 + dx + 1] = neighbors[4];
            }
        }
    }
    float velocity_length = length(velocity * gbuffer_tex_size);
    float hit_dist_atten = hit_distance_attenuation(roughness, camera_dist, neighbors[4].w);
    float anti_flickering_params = anti_flickering_strength * hit_dist_atten;

    float4 moment1 = 0.0;
    float4 moment2 = 0.0;
    for (int i = 0; i < 9; ++i) {
        moment1 += neighbors[i];
        moment2 += neighbors[i] * neighbors[i];
    }
    moment1 /= 9.0;
    moment2 /= 9.0;
    float4 std_dev = sqrt(abs(moment2 - moment1 * moment1));

    const float max_factor_scale = 2.25; // when stationary
    const float min_factor_scale = 0.8; // when moving more than slightly
    float localized_anti_flicker = lerp(
        anti_flickering_params * min_factor_scale,
        anti_flickering_params * max_factor_scale,
        saturate(1.0 - 2.0 * velocity_length)
    );
    float std_dev_multiplier = 1.5;
    std_dev_multiplier += localized_anti_flicker;
    std_dev_multiplier = lerp(std_dev_multiplier, 0.75, saturate(velocity_length / 50.0));

    float4 min_neighbour = moment1 - std_dev * std_dev_multiplier;
    float4 max_neighbour = moment1 + std_dev * std_dev_multiplier;

    prev_lighting_dist = clip_aabb_4d(neighbors[4], prev_lighting_dist, min_neighbour, max_neighbour);
    prev_lighting_dist = lerp(prev_lighting_dist, neighbors[4], 0.05);
    prev_lighting_dist.xyz = ycocg_to_rgb(prev_lighting_dist.xyz);

    out_lighting_dist_tex[pixel_coord] = prev_lighting_dist;
}

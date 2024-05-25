#include <bisemutum/shaders/core/utils/pack.hlsl>

#include "ddgi_struct.hlsl"
#include "probe_blend_common.hlsl"

#include <bisemutum/shaders/core/shader_params/compute.hlsl>

groupshared float4 s_hit_position[DDGI_NUM_RAYS_PER_PROBE];

[numthreads(DDGI_PROBE_VISIBILITY_SIZE, DDGI_PROBE_VISIBILITY_SIZE, 1)]
void ddgi_probe_blend_visibility_cs(
    uint3 global_thread_id : SV_DispatchThreadID,
    uint3 local_thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID
) {
    uint probe_index_linear = group_id.x;
    uint local_thread_index = local_thread_id.y * DDGI_PROBE_VISIBILITY_SIZE + local_thread_id.x;

    uint3 probe_index = uint3(
        probe_index_linear % DDGI_PROBES_SIZE,
        (probe_index_linear / DDGI_PROBES_SIZE) % DDGI_PROBES_SIZE,
        probe_index_linear / DDGI_PROBES_SIZE / DDGI_PROBES_SIZE
    );
    float3 probe_center = volume_base_position
        + probe_index.x * volume_extent_x / DDGI_PROBES_SIZE_M_1 * volume_frame_x
        + probe_index.y * volume_extent_y / DDGI_PROBES_SIZE_M_1 * volume_frame_y
        + probe_index.z * volume_extent_z / DDGI_PROBES_SIZE_M_1 * volume_frame_z;

    uint2 probe_start = uint2(probe_index.y * DDGI_PROBES_SIZE + probe_index.x, probe_index.z) * (DDGI_PROBE_VISIBILITY_SIZE + 2);
    uint2 probe_coord = local_thread_id.xy + 1;
    float3 probe_dir = oct_decode_01((local_thread_id.xy + 0.5) / DDGI_PROBE_VISIBILITY_SIZE);

    [unroll]
    for (uint i = 0; i <= DDGI_NUM_RAYS_PER_PROBE / (DDGI_PROBE_VISIBILITY_SIZE * DDGI_PROBE_VISIBILITY_SIZE); i++) {
        uint ray_index = i * DDGI_PROBE_VISIBILITY_SIZE * DDGI_PROBE_VISIBILITY_SIZE + local_thread_index;
        if (ray_index < DDGI_NUM_RAYS_PER_PROBE) {
            s_hit_position[ray_index] = trace_gbuffer_position.Load(int3(ray_index, probe_index_linear, 0));
        }
    }
    GroupMemoryBarrierWithGroupSync();

    float2 sum = 0.0;
    float weight_sum = 0.0;
    for (uint i = 0; i < DDGI_NUM_RAYS_PER_PROBE; i++) {
        float4 hit_position = s_hit_position[i];
        float trace_dist = hit_position.w < 0.0 ? 1e6 : hit_position.w;
        float3 trace_dir = hit_position.w < 0.0 ? hit_position.xyz : normalize(hit_position.xyz - probe_center);
        float weight = pow(max(dot(probe_dir, trace_dir), 0.0), 50.0);
        sum += weight * float2(trace_dist, trace_dist * trace_dist);
        weight_sum += weight;
    }

    float2 visibility = weight_sum == 0.0 ? 0.0 : sum / weight_sum;
    if (history_valid != 0) {
        float2 hist_visibility = history_probe_visibility.Load(int3(probe_start + probe_coord, 0)).xy;
        visibility = pow(
            lerp(
                pow(visibility, 1.0 / DDGI_TEMPORAL_ACCUMULATE_GAMMA),
                pow(hist_visibility, 1.0 / DDGI_TEMPORAL_ACCUMULATE_GAMMA),
                DDGI_TEMPORAL_ACCUMULATE_ALPHA
            ),
            DDGI_TEMPORAL_ACCUMULATE_GAMMA
        );
    }
    probe_visibility[probe_start + probe_coord] = visibility;

    uint2 probe_border_coord = get_probe_border_coord(probe_coord, DDGI_PROBE_VISIBILITY_SIZE);
    probe_visibility[probe_start + probe_border_coord] = visibility;

    uint2 probe_corner_coord1, probe_corner_coord2;
    if (get_probe_corner_coord(
        probe_coord, DDGI_PROBE_VISIBILITY_SIZE,
        probe_corner_coord1, probe_corner_coord2
    )) {
        probe_visibility[probe_start + probe_corner_coord1] = visibility;
        probe_visibility[probe_start + probe_corner_coord2] = visibility;
    }
}

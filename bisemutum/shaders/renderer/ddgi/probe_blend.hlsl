#include <bisemutum/shaders/core/utils/pack.hlsl>

#include "ddgi_struct.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

uint2 get_probe_border_coord(uint2 probe_coord, uint size) {
    if (probe_coord.x == 1) {
        if (probe_coord.y == 1) {
            return uint2(size + 1, size + 1);
        } else if (probe_coord.y == size) {
            return uint2(size + 1, 0);
        } else {
            return uint2(0, size + 1 - probe_coord.y);
        }
    } else if (probe_coord.x == size) {
        if (probe_coord.y == 1) {
            return uint2(0, size + 1);
        } else if (probe_coord.y == size) {
            return uint2(0, 0);
        } else {
            return uint2(size + 1, size + 1 - probe_coord.y);
        }
    } else if (probe_coord.y == 1) {
        return uint2(size + 1 - probe_coord.x, 0);
    } else if (probe_coord.y == size) {
        return uint2(size + 1 - probe_coord.x, size + 1);
    } else {
        return probe_coord;
    }
}

groupshared float3 s_radiance[DDGI_NUM_RAYS_PER_PROBE];
groupshared float4 s_hit_position[DDGI_NUM_RAYS_PER_PROBE];

[numthreads(DDGI_PROBE_IRRADIANCE_SIZE, DDGI_PROBE_IRRADIANCE_SIZE, 1)]
void ddgi_probe_blend_irradiance_cs(
    uint3 global_thread_id : SV_DispatchThreadID,
    uint3 local_thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID
) {
    uint probe_index_linear = group_id.x;
    uint local_thread_index = local_thread_id.y * DDGI_PROBE_IRRADIANCE_SIZE + local_thread_id.x;

    uint3 probe_index = uint3(
        probe_index_linear % DDGI_PROBES_SIZE,
        (probe_index_linear / DDGI_PROBES_SIZE) % DDGI_PROBES_SIZE,
        probe_index_linear / DDGI_PROBES_SIZE / DDGI_PROBES_SIZE
    );
    float3 probe_center = probe_base_position
        + probe_index.x * probe_extent_x / (DDGI_PROBES_SIZE - 1) * probe_frame_x
        + probe_index.y * probe_extent_y / (DDGI_PROBES_SIZE - 1) * probe_frame_y
        + probe_index.z * probe_extent_z / (DDGI_PROBES_SIZE - 1) * probe_frame_z;

    uint2 probe_coord = local_thread_id.xy + 1;
    float3 probe_dir = oct_decode_01((local_thread_id.xy + 0.5) / DDGI_PROBE_IRRADIANCE_SIZE);

    [unroll]
    for (uint i = 0; i <= DDGI_NUM_RAYS_PER_PROBE / (DDGI_PROBE_IRRADIANCE_SIZE * DDGI_PROBE_IRRADIANCE_SIZE); i++) {
        uint ray_index = i * DDGI_PROBE_IRRADIANCE_SIZE * DDGI_PROBE_IRRADIANCE_SIZE + local_thread_index;
        if (ray_index < DDGI_NUM_RAYS_PER_PROBE) {
            s_radiance[ray_index] = trace_radiance.Load(int3(ray_index, probe_index_linear, 0)).xyz;
            s_hit_position[ray_index] = trace_gbuffer_position.Load(int3(ray_index, probe_index_linear, 0));
        }
    }
    GroupMemoryBarrierWithGroupSync();

    float3 sum = 0.0;
    float weight_sum = 0.0;
    for (uint i = 0; i < DDGI_NUM_RAYS_PER_PROBE; i++) {
        float3 radiance = s_radiance[i];
        float4 hit_position = s_hit_position[i];
        float3 trace_dir = hit_position.w < 0.0 ? hit_position.xyz : normalize(hit_position.xyz - probe_center);
        float weight = max(dot(probe_dir, trace_dir), 0.0);
        sum += weight * radiance;
        weight_sum += weight;
    }

    float3 irradiance = weight_sum == 0.0 ? 0.0 : sum / weight_sum;
    float3 hist_irradiance = probe_irradiance[probe_coord].xyz;
    irradiance = pow(
        lerp(
            pow(irradiance, 1.0 / DDGI_TEMPORAL_ACCUMULATE_GAMMA),
            pow(hist_irradiance, 1.0 / DDGI_TEMPORAL_ACCUMULATE_GAMMA),
            history_valid != 0 ? DDGI_TEMPORAL_ACCUMULATE_ALPHA : 0.0
        ),
        DDGI_TEMPORAL_ACCUMULATE_GAMMA
    );
    probe_irradiance[probe_coord] = float4(irradiance, 1.0);

    uint2 probe_border_coord = get_probe_border_coord(probe_coord, DDGI_PROBE_IRRADIANCE_SIZE);
    probe_irradiance[probe_border_coord] = float4(irradiance, 1.0);
}

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
    float3 probe_center = probe_base_position
        + probe_index.x * probe_extent_x / (DDGI_PROBES_SIZE - 1) * probe_frame_x
        + probe_index.y * probe_extent_y / (DDGI_PROBES_SIZE - 1) * probe_frame_y
        + probe_index.z * probe_extent_z / (DDGI_PROBES_SIZE - 1) * probe_frame_z;

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
    float3 hist_visibility = probe_visibility[probe_coord].xyz;
    visibility = pow(
        lerp(
            pow(visibility, 1.0 / DDGI_TEMPORAL_ACCUMULATE_GAMMA),
            pow(hist_visibility, 1.0 / DDGI_TEMPORAL_ACCUMULATE_GAMMA),
            history_valid != 0 ? DDGI_TEMPORAL_ACCUMULATE_ALPHA : 0.0
        ),
        DDGI_TEMPORAL_ACCUMULATE_GAMMA
    );
    probe_visibility[probe_coord] = visibility;

    uint2 probe_border_coord = get_probe_border_coord(probe_coord, DDGI_PROBE_VISIBILITY_SIZE);
    probe_visibility[probe_border_coord] = visibility;
}

#include <bisemutum/shaders/core/utils/random.hlsl>
#include <bisemutum/shaders/core/utils/sampling.hlsl>

#include "../raytracing/hits/rt_gbuffer.hlsl"
#include "ddgi_struct.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/raytracing.hlsl>

[shader("raygeneration")]
void ddgi_trace_gbuffer_rgen() {
    uint probe_index_linear = DispatchRaysIndex().x;
    uint ray_index = DispatchRaysIndex().y;

    uint3 probe_index = uint3(
        probe_index_linear % DDGI_PROBES_SIZE,
        (probe_index_linear / DDGI_PROBES_SIZE) % DDGI_PROBES_SIZE,
        probe_index_linear / DDGI_PROBES_SIZE / DDGI_PROBES_SIZE
    );
    float3 probe_center = volume_base_position
        + probe_index.x * volume_extent_x / (DDGI_PROBES_SIZE - 1) * volume_frame_x
        + probe_index.y * volume_extent_y / (DDGI_PROBES_SIZE - 1) * volume_frame_y
        + probe_index.z * volume_extent_z / (DDGI_PROBES_SIZE - 1) * volume_frame_z;

    uint rng_seed = rng_tea(probe_index_linear, frame_index);
    uint rand_index = (uint(rng_next(rng_seed) * DDGI_SAMPLE_RANDOM_SIZE) + ray_index) % DDGI_SAMPLE_RANDOM_SIZE;
    float2 sample_random = sample_randoms[rand_index];

    float3 ray_dir = uniform_sphere_sample(sample_random);

    RayDesc ray;
    ray.TMin = 0.001;
    ray.TMax = ray_length;
    ray.Origin = probe_center;
    ray.Direction = ray_dir;

    GBufferPayload gbuffer;
    gbuffer.opacity_random = gen_opacity_random_for(probe_center, ray_dir);
    TraceRay(scene_accel, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, gbuffer);

    uint2 output_coord = uint2(ray_index, probe_index_linear);
    if (gbuffer.hit_t > 0.0) {
        probe_gbuffer_base_color[output_coord] = gbuffer.gbuffer.base_color;
        probe_gbuffer_normal_roughness[output_coord] = gbuffer.gbuffer.normal_roughness;
        probe_gbuffer_position[output_coord] = float4(probe_center + ray_dir * gbuffer.hit_t, gbuffer.hit_t);
    } else {
        probe_gbuffer_base_color[output_coord] = 0.0;
        probe_gbuffer_normal_roughness[output_coord] = 0.0;
        probe_gbuffer_position[output_coord] = float4(ray_dir, -1.0);
    }
}

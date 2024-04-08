#include <bisemutum/shaders/core/vertex_attributes.hlsl>
#include <bisemutum/shaders/core/utils/random.hlsl>
#include <bisemutum/shaders/core/utils/sampling.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/frame.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#define NUM_GROUP_THREADS 16

[numthreads(NUM_GROUP_THREADS, NUM_GROUP_THREADS, 1)]
void ambient_occlusion_rt_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    uint2 pixel_coord = global_thread_id.xy;
    if (any(pixel_coord >= tex_size)) { return; }
    float2 texcoord = (pixel_coord + 0.5) / tex_size;

    float depth = depth_tex.SampleLevel(input_sampler, texcoord, 0).x;
    if (depth == 1.0) {
        ao_tex[pixel_coord] = float2(1.0, 0.0);
        return;
    }

    float4 normal_roughness = normal_roughness_tex.SampleLevel(input_sampler, texcoord, 0);
    float3 normal;
    float3 tangent;
    unpack_normal_and_tangent(normal_roughness.xyz, normal, tangent);
    Frame frame = create_frame(normal, tangent);

    float3 position_view = position_view_from_depth(texcoord, depth, matrix_inv_proj);
    float3 position_world = mul(matrix_inv_view, float4(position_view, 1.0)).xyz;

    uint rng_seed = rng_tea(pixel_coord.y * tex_size.x + pixel_coord.x, frame_index);
    float ao_value = 0.0;
    [unroll]
    for (uint i = 0; i < 4; i++) {
        float2 rand = float2(rng_next(rng_seed), rng_next(rng_seed));
        float3 dir_local = cos_hemisphere_sample(rand);
        float3 dir_world = frame_to_world(frame, dir_local);

        RayDesc ray;
        ray.TMin = 0.001f;
        ray.TMax = ao_range;
        ray.Origin = position_world + normal * 0.001f;
        ray.Direction = dir_world;

        RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ray_query;
        ray_query.TraceRayInline(scene_accel, 0, 0xff, ray);
        ray_query.Proceed();

        ao_value += ray_query.CommittedStatus() == COMMITTED_NOTHING ? 0.0 : 1.0;
    }
    ao_value = 1.0 - ao_value * ao_strength / 4;

    ao_tex[pixel_coord] = float2(ao_value, 1.0);
}

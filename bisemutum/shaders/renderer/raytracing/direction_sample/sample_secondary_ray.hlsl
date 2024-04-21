#include <bisemutum/shaders/core/utils/random.hlsl>
#include <bisemutum/shaders/core/utils/sampling.hlsl>
#include <bisemutum/shaders/core/utils/frame.hlsl>
#include <bisemutum/shaders/core/material.hlsl>

#include "../../gbuffer.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

[numthreads(16, 16, 1)]
void sample_secondary_ray_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    uint2 pixel_coord = global_thread_id.xy;
    if (any(pixel_coord >= tex_size)) { return; }

    float4 new_ray_origin = hit_positions.Load(int3(pixel_coord, 0));
    if (new_ray_origin.w < 0.0) {
        ray_directions[pixel_coord] = 0.0;
        ray_weights[pixel_coord] = 0.0;
        return;
    }

    float3 last_weight = ray_weights[pixel_coord].xyz;
    if (max(last_weight.x, max(last_weight.y, last_weight.z)) < 0.001) {
        ray_directions[pixel_coord] = 0.0;
        ray_weights[pixel_coord] = 0.0;
        return;
    }

    GBuffer gbuffer;
    gbuffer.base_color = gbuffer_textures[GBUFFER_BASE_COLOR].Load(int3(pixel_coord, 0));
    gbuffer.normal_roughness = gbuffer_textures[GBUFFER_NORMAL_ROUGHNESS].Load(int3(pixel_coord, 0));
    gbuffer.fresnel = gbuffer_textures[GBUFFER_FRESNEL].Load(int3(pixel_coord, 0));
    gbuffer.material_0 = gbuffer_textures[GBUFFER_MATERIAL_0].Load(int3(pixel_coord, 0));

    float3 N, T;
    SurfaceData surface;
    uint surface_model;
    unpack_gbuffer_to_surface(gbuffer, N, T, surface, surface_model);

    float3 B = cross(N, T);
    Frame frame = create_frame(N, T);

    float3 last_ray_origin = ray_origins[pixel_coord].xyz;
    float3 V = normalize(last_ray_origin - new_ray_origin.xyz);
    float3 V_local = frame_to_local(frame, V);

    float roughness_x, roughness_y;
    get_anisotropic_roughness(surface.roughness, surface.anisotropy, roughness_x, roughness_y);

    // TODO - sample full bsdf
    uint rng_seed = rng_tea(pixel_coord.y * tex_size.x + pixel_coord.x, frame_index + bounce_index * 3);
    float3 half_dir = ggx_vndf_sample(V_local, roughness_x, roughness_y, float2(rng_next(rng_seed), rng_next(rng_seed)));
    float3 out_dir = reflect(-V_local, half_dir);
    float pdf_wh = ggx_vndf_sample_pdf(half_dir, V_local, roughness_x, roughness_y);
    float pdf = pdf_wh / (4.0 * abs(dot(half_dir, V_local)));

    out_dir = frame_to_world(frame, out_dir);
    float3 bsdf = surface_eval(N, T, B, V, out_dir, surface, surface_model);

    float3 weight = bsdf / pdf;
    if (any(isnan(weight)) || any(isinf(weight))) {
        weight = 0.0;
    }

    ray_origins[pixel_coord] = float4(new_ray_origin.xyz, 1.0);
    ray_directions[pixel_coord] = float4(out_dir, 1.0);
    ray_weights[pixel_coord]  = float4(weight * last_weight, 1.0);
}

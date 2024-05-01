#include <bisemutum/shaders/core/utils/random.hlsl>
#include <bisemutum/shaders/core/utils/sampling.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/frame.hlsl>
#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/material.hlsl>

#include "../../gbuffer.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

[numthreads(16, 16, 1)]
void specular_sample_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    uint2 pixel_coord = global_thread_id.xy;
    if (any(pixel_coord >= tex_size)) { return; }
#if !HALF_RESOLUTION
    float2 texcoord = (pixel_coord + 0.5) / tex_size;
#else
    float2 sub_pixel = float2(
        (frame_index & 1) != 0 ? 0.75 : 0.25,
        (frame_index & 2) != 0 ? 0.75 : 0.25
    );
    float2 texcoord = (pixel_coord + sub_pixel) / tex_size;
#endif

    float depth = depth_tex.SampleLevel(gbuffer_sampler, texcoord, 0).x;
    if (is_depth_background(depth)) {
        ray_directions[pixel_coord] = 0.0;
        ray_weights[pixel_coord] = 0.0;
        return;
    }

    GBuffer gbuffer;
    gbuffer.base_color = gbuffer_textures[GBUFFER_BASE_COLOR].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.normal_roughness = gbuffer_textures[GBUFFER_NORMAL_ROUGHNESS].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.fresnel = gbuffer_textures[GBUFFER_FRESNEL].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.material_0 = gbuffer_textures[GBUFFER_MATERIAL_0].SampleLevel(gbuffer_sampler, texcoord, 0);

    float3 N, T;
    SurfaceData surface;
    uint surface_model;
    unpack_gbuffer_to_surface(gbuffer, N, T, surface, surface_model);

    if (surface.roughness > max_roughness) {
        ray_directions[pixel_coord] = 0.0;
        ray_weights[pixel_coord] = 0.0;
        return;
    }

    float3 B = cross(N, T);
    Frame frame = create_frame(N, T);

    float3 position_view = position_view_from_depth(texcoord, depth, matrix_inv_proj);
    float3 position_world = mul(matrix_inv_view, float4(position_view, 1.0)).xyz;
    float3 V = normalize(camera_position_world() - position_world);
    float3 V_local = frame_to_local(frame, V);

    float roughness_x, roughness_y;
    get_anisotropic_roughness(surface.roughness, surface.anisotropy, roughness_x, roughness_y);

    uint rng_seed = rng_tea(pixel_coord.y * tex_size.x + pixel_coord.x, frame_index);
    float3 half_dir = ggx_vndf_sample(V_local, roughness_x, roughness_y, float2(rng_next(rng_seed), rng_next(rng_seed)));
    float3 out_dir = reflect(-V_local, half_dir);
    float pdf_wh = ggx_vndf_sample_pdf(half_dir, V_local, roughness_x, roughness_y);
    float pdf = pdf_wh / (4.0 * abs(dot(half_dir, V_local)));

    out_dir = frame_to_world(frame, out_dir);
    float3 bsdf_diffuse;
    float3 bsdf_specular;
    surface_eval(N, T, B, V, out_dir, surface, surface_model, bsdf_diffuse, bsdf_specular);
    float fade = 1.0 - max(surface.roughness - fade_roughness, 0.0) / max(max_roughness - fade_roughness, 0.0001);

    float3 weight = bsdf_specular * fade / pdf;
    if (any(isnan(weight)) || any(isinf(weight))) {
        weight = 0.0;
    }

    ray_origins[pixel_coord] = float4(position_world, 1.0);
    ray_directions[pixel_coord] = float4(out_dir, 1.0);
    ray_weights[pixel_coord]  = float4(weight, 1.0);
}

#include "../../../core/utils/random.hlsl"
#include "../../../core/utils/sampling.hlsl"
#include "../../../core/utils/projection.hlsl"
#include "../../../core/utils/pack.hlsl"
#include "../../../core/utils/frame.hlsl"
#include "../../../core/material.hlsl"
#include "../../gbuffer.hlsl"

#include "../../../core/shader_params/camera.hlsl"
#include "../../../core/shader_params/compute.hlsl"

[numthreads(16, 16, 1)]
void specular_sample_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    uint2 pixel_coord = global_thread_id.xy;
    if (any(pixel_coord >= tex_size)) { return; }
    float2 texcoord = (pixel_coord + 0.5) / tex_size;

    float depth = depth_tex.SampleLevel(gbuffer_sampler, texcoord, 0).x;
    if (depth == 1.0) {
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
    float3 bsdf = surface_eval(N, T, B, V, out_dir, surface, surface_model);
    // TODO - split specular bsdf
    float fade = 1.0 - max(surface.roughness - fade_roughness, 0.0) / max(max_roughness - fade_roughness, 0.0001);

    ray_origins[pixel_coord] = float4(position_world, 1.0);
    ray_directions[pixel_coord] = float4(out_dir, 1.0);
    ray_weights[pixel_coord]  = float4(bsdf * fade / pdf, 1.0);
}

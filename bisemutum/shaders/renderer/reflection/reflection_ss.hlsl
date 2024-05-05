#include <bisemutum/shaders/core/utils/random.hlsl>
#include <bisemutum/shaders/core/utils/sampling.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/frame.hlsl>
#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/material.hlsl>

#include "../gbuffer.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

[numthreads(16, 16, 1)]
void reflection_ss_cs(uint3 global_thread_id : SV_DispatchThreadID) {
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
        reflection_tex[pixel_coord] = 0.0;
        return;
    }

    GBuffer gbuffer;
    gbuffer.normal_roughness = gbuffer_textures[GBUFFER_NORMAL_ROUGHNESS].SampleLevel(gbuffer_sampler, texcoord, 0);
    if (gbuffer.normal_roughness.w > max_roughness) {
        reflection_tex[pixel_coord] = 0.0;
        return;
    }
    gbuffer.base_color = gbuffer_textures[GBUFFER_BASE_COLOR].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.fresnel = gbuffer_textures[GBUFFER_FRESNEL].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.material_0 = gbuffer_textures[GBUFFER_MATERIAL_0].SampleLevel(gbuffer_sampler, texcoord, 0);

    float3 N, T;
    SurfaceData surface;
    uint surface_model;
    unpack_gbuffer_to_surface(gbuffer, N, T, surface, surface_model);
    float3 B = cross(N, T);
    Frame frame = create_frame(N, T);

    float3 position_view = position_view_from_depth(texcoord, depth, matrix_inv_proj);
    float3 position_world = mul(matrix_inv_view, float4(position_view, 1.0)).xyz;
    float3 V = normalize(camera_position_world() - position_world);
    float3 V_local = frame_to_local(frame, V);
    float3 ray_org = float3(texcoord, get_linear_01_depth(depth, matrix_inv_proj));

    uint rng_seed = rng_tea(pixel_coord.y * tex_size.x + pixel_coord.x, frame_index);
    float roughness_x, roughness_y;
    get_anisotropic_roughness(surface.roughness, surface.anisotropy, roughness_x, roughness_y);
    float3 half_dir = ggx_vndf_sample(V_local, roughness_x, roughness_y, float2(rng_next(rng_seed), rng_next(rng_seed)));
    float3 out_dir = reflect(-V_local, half_dir);
    float pdf_wh = ggx_vndf_sample_pdf(half_dir, V_local, roughness_x, roughness_y);
    float pdf = pdf_wh / (4.0 * abs(dot(half_dir, V_local)));

    if (out_dir.z <= 0.0) {
        hit_position_tex[pixel_coord] = 0.0;
        reflection_tex[pixel_coord] = 0.0;
        return;
    }

    out_dir = frame_to_world(frame, out_dir);
    float4 out_pos_clip = mul(matrix_proj_view, float4(position_world + out_dir, 1.0));
    out_pos_clip /= out_pos_clip.w;
    float2 out_pos_uv = float2(out_pos_clip.x * 0.5 + 0.5, 0.5 - out_pos_clip.y * 0.5);
    float3 ray_dir = float3(out_pos_uv - ray_org.xy, get_linear_01_depth(out_pos_clip.z, matrix_inv_proj) - ray_org.z);
    float3 inv_ray_dir = float3(
        abs(ray_dir.x) < 1e-7 ? 1e9 : 1.0 / ray_dir.x,
        abs(ray_dir.y) < 1e-7 ? 1e9 : 1.0 / ray_dir.y,
        abs(ray_dir.z) < 1e-7 ? 1e9 : 1.0 / ray_dir.z
    );

    float2 saturated_ray_dir_sign = float2(ray_dir.x > 0.0 ? 1.0 : 0.0, ray_dir.y > 0.0 ? 1.0 : 0.0);

    uint curr_level = 1;
    uint num_iterations = 0;
    float3 curr_pos = ray_org;
    float hit_t = 0.0;
    {
        uint2 pyramid_tex_size = max(pyramid_level0_tex_size >> curr_level, 1u);
        float2 texel_bound = (floor(curr_pos.xy * pyramid_tex_size) + saturated_ray_dir_sign) / pyramid_tex_size;
        float2 t_uv = (texel_bound - curr_pos.xy) * inv_ray_dir.xy;
        float t = min(t_uv.x, t_uv.y) + 0.0001;
        curr_pos += t * ray_dir;
        hit_t += t;
    }
    while (num_iterations < max_num_iterations && all(saturate(curr_pos.xy) == curr_pos.xy)) {
        uint2 pyramid_tex_size = max(pyramid_level0_tex_size >> curr_level, 1u);
        int2 pyramid_pixel_coord = min(int2(curr_pos.xy * pyramid_tex_size), pyramid_tex_size - 1);
        float nearest_depth = get_linear_01_depth(depth_tex.Load(int3(pyramid_pixel_coord, curr_level)).x, matrix_inv_proj);
        float2 texel_bound = (pyramid_pixel_coord + saturated_ray_dir_sign) / pyramid_tex_size;
        float2 t_uv = (texel_bound - curr_pos.xy) * inv_ray_dir.xy;
        float t = min(t_uv.x, t_uv.y) + 0.0001;
        if (ray_dir.z > 0.0) {
            float t_depth = max(nearest_depth - curr_pos.z, 0.0) * inv_ray_dir.z;
            if (t < t_depth) {
                curr_level = min(curr_level + 2, pyramid_max_level);
            } else {
                t = t_depth;
            }
        } else if (curr_pos.z <= nearest_depth) {
            curr_level = min(curr_level + 2, pyramid_max_level);
        } else {
            t = 0.0;
        }
        curr_pos += t * ray_dir;
        hit_t += t;
        if (curr_level > 0) --curr_level;
        ++num_iterations;
    }

    bool has_hit = false;
    if (all(saturate(curr_pos.xy) == curr_pos.xy)) {
        float nearest_depth = get_linear_01_depth(depth_tex.Load(int3(curr_pos.xy * pyramid_level0_tex_size, 0)).x, matrix_inv_proj);
        float diff = curr_pos.z - nearest_depth;
        has_hit = diff >= 0.0 && diff <= ssr_thickness;
    }

    if (has_hit) {
        float3 bsdf_diffuse;
        float3 bsdf_specular;
        surface_eval(N, T, B, V, out_dir, surface, surface_model, bsdf_diffuse, bsdf_specular);

        float3 color = bsdf_specular * color_tex.SampleLevel(gbuffer_sampler, curr_pos.xy, 0).xyz;
        float fade = 1.0 - max(surface.roughness - fade_roughness, 0.0) / max(max_roughness - fade_roughness, 0.0001);

        reflection_tex[pixel_coord] = float4(color * fade / pdf, 1.0);
        hit_position_tex[pixel_coord] = float4(position_world + out_dir * hit_t, hit_t);
    } else {
        reflection_tex[pixel_coord] = 0.0;
        hit_position_tex[pixel_coord] = float4(out_dir, -1.0);
    }
}

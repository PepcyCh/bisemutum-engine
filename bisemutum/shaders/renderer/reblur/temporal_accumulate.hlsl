#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "utils.hlsl"
#include "../validate_history_flags.hlsl"

[numthreads(REBLUR_TILE_SIZE, REBLUR_TILE_SIZE, 1)]
void temporal_accumulate_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    const int2 pixel_coord = int2(global_thread_id.xy);
    const float2 texel_size = 1.0 / tex_size;
    const float2 center_uv = (pixel_coord + 0.5) * texel_size;

#if REBLUR_USE_HALF_RESOLUTION
    const int2 gbuffer_pixel_coord = pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);
#else
    const int2 gbuffer_pixel_coord = pixel_coord;
#endif

    if (any(pixel_coord >= tex_size)) return;

    float depth = depth_tex.Load(int3(gbuffer_pixel_coord, 0)).x;
    uint validation_mask = history_validation_tex[pixel_coord];
    float4 lighting_dist = in_lighting_dist_tex.Load(int3(pixel_coord, 0));
    if (is_depth_background(depth) || validation_mask != 0) {
        out_lighting_dist_tex[pixel_coord] = lighting_dist;
        accumulation_tex[pixel_coord] = 0.0;
        return;
    }

    float4 normal_roughness = normal_roughness_tex.Load(int3(pixel_coord, 0));
    float3 normal = oct_decode(normal_roughness.xy);
    float roughness = normal_roughness.w;

    float3 position = position_world_from_depth(center_uv, depth, matrix_inv_proj, matrix_inv_view);
    float3 view = normalize(camera_position_world() - position);
    float ndotv = dot(normal, view);

    float2 velocity = velocity_tex.Load(int3(gbuffer_pixel_coord, 0)).xy;
    float2 prev_uv = center_uv - velocity;
    float2 prev_gbuffer_pixel_coord_f = prev_uv * gbuffer_tex_size;
    int2 prev_gbuffer_pixel_coord = int2(prev_gbuffer_pixel_coord_f);
    float hist_depth = hist_depth_tex.Load(int3(prev_gbuffer_pixel_coord, 0)).x;
    float3 hist_position = position_world_from_depth(prev_uv, hist_depth, history_matrix_inv_proj, history_matrix_inv_view);
    float3 hist_view = normalize(history_camera_position_world() - hist_position);

    float parallax = calc_parallax(view, hist_view);

    float4 hist_lighting_dist = hist_lighting_dist_tex.SampleLevel(input_sampler, prev_uv, 0);
    float accum_hist = hist_accumulation_tex.SampleLevel(input_sampler, prev_uv, 0).x;
    float accum_factor = get_specular_accum_speed(roughness, ndotv, parallax);
    accum_factor = min(min(accum_factor, accum_hist), REBLUR_NUM_MAX_ACCUM_FRAME);
    float4 lerped_lighting_dist = lerp(hist_lighting_dist, lighting_dist, 1.0 / (1.0 + accum_factor));

    if (virtual_history != 0) {
        float3 virtual_position = get_virtual_position(position, view, ndotv, roughness, lerped_lighting_dist.w);
        float4 virtual_position_clip = mul(history_matrix_proj_view, float4(virtual_position, 1.0));
        virtual_position_clip /= virtual_position_clip.w;
        float2 virtual_position_uv = float2(virtual_position_clip.x * 0.5 + 0.5, 0.5 - virtual_position_clip.y * 0.5);
        bool is_virtual_depth_valid = virtual_position_clip.z >= -1.0 && virtual_position_clip.z <= 1.0;
        if (all(saturate(virtual_position_uv) == virtual_position_uv) && is_virtual_depth_valid) {
            int2 virtual_gbuffer_pixel_coord = int2(virtual_position_uv * gbuffer_tex_size);
            float hist_virtual_depth = hist_depth_tex.Load(int3(virtual_gbuffer_pixel_coord, 0)).x;
            float4 hist_virtual_lighting_dist = hist_lighting_dist_tex.SampleLevel(input_sampler, virtual_position_uv, 0);

            float amount = get_specular_dominant_factor(ndotv, roughness);
            float confidence = 1.0;

            float linear_depth = get_linear_01_depth(depth, matrix_inv_proj);
            float virtual_linear_depth = get_linear_01_depth(hist_virtual_depth, history_matrix_inv_proj);
            amount *= abs(linear_depth - virtual_linear_depth) < linear_depth * 0.1 ? 1.0 : 0.0;
            float3 hist_virtial_normal = oct_decode(hist_normal_roughness_tex.Load(int3(virtual_gbuffer_pixel_coord, 0)).xy);
            amount *= dot(normal, hist_virtial_normal) > 0.9 ? 1.0 : 0.0;

            float a_virtual = get_specular_accum_speed(roughness, ndotv, 0.0);
            float a_min = min(a_virtual, REBLUR_NUM_MIP_LEVEL * sqrt(roughness));
            float a = lerp(1.0 / (1.0 + a_min), 1.0 / (1.0 + a_virtual), confidence);
            a_virtual = 1.0 / a - 1.0;

            float a_hit_dist = min(a_virtual, REBLUR_NUM_MAX_ACCUM_FRAME);
            hist_virtual_lighting_dist.xyz = lerp(hist_virtual_lighting_dist.xyz, lighting_dist.xyz, 1.0 / (1.0 + a_virtual));
            hist_virtual_lighting_dist.w = lerp(hist_virtual_lighting_dist.w, lighting_dist.w, 1.0 / (1.0 + a_hit_dist));

            float4 result = lerp(lerped_lighting_dist, hist_virtual_lighting_dist, amount);

            a = lerp(1.0 / (1.0 + accum_factor), 1.0 / (1.0 + a_virtual), amount);
            float a_curr = 1.0 / a - 1.0;

            lerped_lighting_dist = result;
            accum_factor = a_curr;
        }
    }

    out_lighting_dist_tex[pixel_coord] = lerped_lighting_dist;
    accumulation_tex[pixel_coord] = accum_factor;
}

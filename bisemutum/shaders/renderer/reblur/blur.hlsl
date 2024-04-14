#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "filter.hlsl"

#define BLUR_MAX_RADIUS 0.04
#define MIN_BLUR_DISTANCE 0.03
#define BLUR_OUT_RANGE 0.05

[numthreads(REBLUR_TILE_SIZE, REBLUR_TILE_SIZE, 1)]
void blur_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    const int2 pixel_coord = int2(global_thread_id.xy);
    const float2 texel_size = 1.0 / tex_size;
    const float2 center_uv = (pixel_coord + 0.5) * texel_size;
    const float2 gbuffer_texel_size = 1.0 / gbuffer_tex_size;

#if REBLUR_USE_HALF_RESOLUTION
    const int2 gbuffer_pixel_coord = pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);
#else
    const int2 gbuffer_pixel_coord = pixel_coord;
#endif

    if (any(pixel_coord >= tex_size)) return;

    BilateralData center = tap_bilateral_data(
        gbuffer_pixel_coord,
        (gbuffer_pixel_coord + 0.5) * gbuffer_texel_size,
        depth_tex,
        normal_roughness_tex,
        matrix_inv_proj,
        matrix_inv_view
    );
    if (center.z_01 > 0.999) {
        out_lighting_dist_tex[pixel_coord] = float4(0.0, 0.0, 0.0, -1.0);
        return;
    }

    float4 center_lighting_dist = in_lighting_dist_tex.Load(int3(pixel_coord, 0));
    float accum = accumulation_tex.Load(int3(pixel_coord, 0)).x;

    float3 view_vec = camera_position_world() - center.position;
    float camera_dist = length(view_vec);
    float3 view = view_vec / camera_dist;
    float ndotv = dot(center.normal, view);
    float4 dominant = get_specular_dominant_direction(center.normal, view, center.roughness);

    float blur_radius = calc_blur_radius(center.roughness, BLUR_MAX_RADIUS) * in_blur_radius
        * (1.0 - saturate(accum / REBLUR_NUM_MAX_ACCUM_FRAME))
        * hit_distance_attenuation(center.roughness, camera_dist, center_lighting_dist.w)
        * saturate((camera_dist - MIN_BLUR_DISTANCE) / BLUR_OUT_RANGE);

    float2x3 TvBv = get_kernel_basis(dominant.xyz, center.normal, center.roughness);
    TvBv *= blur_radius;

    float4 sum = 0.0;
    float sum_weight = 0.0;
    for (uint i = 0; i < NUM_POISSON_SAMPLE; i++) {
        float3 offset = poisson_disk_samples[i];

        float2 rotated_offset = rotate_vector(poisson_rotator, offset.xy);
        float3 position_i = center.position + TvBv[0] * rotated_offset.x + TvBv[1] * rotated_offset.y;
        float4 position_clip = mul(matrix_proj_view, float4(position_i, 1.0));
        position_clip /= position_clip.w;
        float2 uv = float2(position_clip.x * 0.5 + 0.5, 0.5 - position_clip.y * 0.5);

        int2 tap_pixel_coord = clamp(int2(uv * tex_size), 0, tex_size - 1);
#if REBLUR_USE_HALF_RESOLUTION
        const int2 tap_gbuffer_pixel_coord = tap_pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);
#else
        const int2 tap_gbuffer_pixel_coord = tap_pixel_coord;
#endif
        BilateralData tap_data = tap_bilateral_data(
            tap_gbuffer_pixel_coord,
            uv,
            depth_tex,
            normal_roughness_tex,
            matrix_inv_proj,
            matrix_inv_view
        );
        float w = get_gaussian_weight(offset.z)
            * calc_bilateral_weight(center, tap_data)
            * (tap_data.z_01 < 0.999 ? 1.0 : 0.0)
            * (all(saturate(uv) == uv) ? 1.0 : 0.0);
        float4 tap_lighting_dist = in_lighting_dist_tex.Load(int3(tap_pixel_coord, 0));

        sum += tap_lighting_dist * w;
        sum_weight += w;
    }

    float4 result = sum_weight == 0.0 ? center_lighting_dist : sum / sum_weight;
    out_lighting_dist_tex[pixel_coord] = result;
}

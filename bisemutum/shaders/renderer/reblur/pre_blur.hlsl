#include <bisemutum/shaders/core/utils/color.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "filter.hlsl"

#define PRE_BLUR_MAX_RADIUS 20.0
#define PRE_BLUR_CLAMP_LUM 1.5

[numthreads(REBLUR_TILE_SIZE, REBLUR_TILE_SIZE, 1)]
void pre_blur_cs(uint3 global_thread_id : SV_DispatchThreadID) {
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

    float center_dist = in_dist.Load(int3(pixel_coord, 0)).w;

    float blur_radius = calc_blur_radius(center.roughness, PRE_BLUR_MAX_RADIUS) * in_blur_radius;
    float camera_dist = length(center.position - camera_position_world());
    blur_radius *= hit_distance_attenuation(center.roughness, camera_dist, center_dist);

    float4 sum = 0.0;
    float sum_weight = 0.0;
    for (uint i = 0; i < NUM_POISSON_SAMPLE; i++) {
        float3 offset = poisson_disk_samples[i];
        float2 uv = center_uv + rotate_vector(poisson_rotator, offset.xy) * gbuffer_texel_size * blur_radius;

        int2 tap_pixel_coord = clamp(int2(uv * tex_size), 0, tex_size - 1);
#if REBLUR_USE_HALF_RESOLUTION
        const int2 tap_gbuffer_pixel_coord = tap_pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);
#else
        const int2 tap_gbuffer_pixel_coord = tap_pixel_coord;
#endif
        BilateralData tap_data = tap_bilateral_data(
            tap_gbuffer_pixel_coord,
            (tap_gbuffer_pixel_coord + 0.5) * gbuffer_texel_size,
            depth_tex,
            normal_roughness_tex,
            matrix_inv_proj,
            matrix_inv_view
        );

        float w = get_gaussian_weight(offset.z)
            * calc_bilateral_weight(center, tap_data)
            * (tap_data.z_01 < 0.999 ? 1.0 : 0.0)
            * (all(saturate(uv) == uv) ? 1.0 : 0.0);

        float3 lighting = max(in_color.Load(int3(tap_pixel_coord, 0)).xyz, 0.0);
        if (any(isnan(lighting)) || any(isinf(lighting))) { lighting = 0.0; }
        float dist = in_dist.Load(int3(tap_pixel_coord, 0)).w;
        float lum = luminance(lighting);
        float scale = min(lum, PRE_BLUR_CLAMP_LUM) / max(lum, 0.0001);
        lighting *= scale;

        sum += float4(lighting, dist) * w;
        sum_weight += w;
    }

    float4 blurred;
    if (sum_weight != 0.0) {
        blurred = sum / sum_weight;
    } else {
        float3 lighting = max(in_color.Load(int3(pixel_coord, 0)).xyz, 0.0);
        if (any(isnan(lighting)) || any(isinf(lighting))) { lighting = 0.0; };
        float lum = luminance(lighting);
        float scale = min(lum, PRE_BLUR_CLAMP_LUM) / max(lum, 0.0001);
        lighting *= scale;
        blurred = float4(lighting, center_dist);
    }
    blurred.w = clamp(blurred.w, 0.0, 16.0);

    out_lighting_dist_tex[pixel_coord] = blurred;
}

#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "utils.hlsl"

float gaussian(float radius, float sigma) {
    float v = radius / sigma;
    return exp(-(v * v));
}

[numthreads(REBLUR_TILE_SIZE, REBLUR_TILE_SIZE, 1)]
void fix_history_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    const int2 pixel_coord = int2(global_thread_id.xy);
    const float2 texel_size = 1.0 / tex_size;
    const float2 center_uv = (pixel_coord + 0.5) * texel_size;

#if REBLUR_USE_HALF_RESOLUTION
    const int2 gbuffer_pixel_coord = pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);
#else
    const int2 gbuffer_pixel_coord = pixel_coord;
#endif

    if (any(pixel_coord >= tex_size)) return;

    float linear_depth = depth_tex.Load(int3(pixel_coord, 0)).x;
    if (linear_depth > 0.999) return;

    float4 lighting_dist = in_lighting_dist_tex.Load(int3(pixel_coord, 0));
    float accum = accumulation_tex.Load(int3(pixel_coord, 0)).x;
    float norm_accum = saturate(accum / REBLUR_NUM_MAX_FRAME_WITH_HISTORY_FIX);
    if (norm_accum == 1.0) {
        out_lighting_dist_tex[pixel_coord] = lighting_dist;
        return;
    }

    float roughness = normal_roughness_tex.Load(int3(gbuffer_pixel_coord, 0)).w;
    int mip_level = min(int(REBLUR_NUM_MIP_LEVEL * (1.0 - norm_accum) * roughness), int(REBLUR_NUM_MIP_LEVEL - 1));

    float4 sum = 0.0;
    float sum_weight = 0.0;
    while (mip_level >= 0) {
        int2 mip_pixel_coord = pixel_coord >> mip_level;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int2 tap_pixel_coord = mip_pixel_coord + int2(dx, dy);
                float tap_linear_depth = depth_tex.Load(int3(tap_pixel_coord, mip_level)).x;
                float4 tap_data = in_lighting_dist_tex.Load(int3(tap_pixel_coord, mip_level));
                if (abs(linear_depth - tap_linear_depth) < linear_depth * 0.1 && tap_data.w > 0.0) {
                    float w = gaussian(length(float2(dx, dy)), 1.0);
                    sum += tap_data * w;
                    sum_weight += w;
                }
            }
        }
        if (sum_weight > 3.5) {
            break;
        }
        --mip_level;
    }

    float4 result = (sum_weight == 0.0 || mip_level < 0) ? lighting_dist : sum / sum_weight;
    out_lighting_dist_tex[pixel_coord] = result;
}

#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/utils/bits.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "utils.hlsl"

#define TILE_SIZE 16
#define NUM_SHARED_DATA (TILE_SIZE * TILE_SIZE / 4)

groupshared float4 s_data[NUM_SHARED_DATA];
groupshared float s_depth[NUM_SHARED_DATA];

float4 safe_texel_fetch(RWTexture2D<float4> tex, int2 pixel_coord) {
    return tex[clamp(pixel_coord, 0, tex_size - 1)];
}
float safe_texel_fetch(RWTexture2D<float> tex, int2 pixel_coord) {
    return tex[clamp(pixel_coord, 0, tex_size - 1)];
}

float calc_weight(float depth, float mip_depth, float4 lighting_dist) {
    return (abs(depth - mip_depth) < mip_depth * 0.1 && lighting_dist.w > 0.0) ? 1.0 : 0.0;
}

[numthreads(64, 1, 1)]
void gen_depth_mip_cs(
    uint3 global_thread_id : SV_DispatchThreadID,
    uint3 local_thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID
) {
    const int2 group_index = int2(group_id.xy);
    const uint local_index = uint(local_thread_id.x);

    uint local_x = extract_even_bits(local_index);
    uint local_y = extract_even_bits(local_index >> 1);
    int2 pixel_coord = group_index * TILE_SIZE + int2(local_x * 2, local_y * 2);

    float4 v0 = safe_texel_fetch(in_tex, pixel_coord);
    float4 v1 = safe_texel_fetch(in_tex, pixel_coord + int2(1, 0));
    float4 v2 = safe_texel_fetch(in_tex, pixel_coord + int2(0, 1));
    float4 v3 = safe_texel_fetch(in_tex, pixel_coord + int2(1, 1));

    float d0 = safe_texel_fetch(in_depth_tex, pixel_coord);
    float d1 = safe_texel_fetch(in_depth_tex, pixel_coord + int2(1, 0));
    float d2 = safe_texel_fetch(in_depth_tex, pixel_coord + int2(0, 1));
    float d3 = safe_texel_fetch(in_depth_tex, pixel_coord + int2(1, 1));
    float dm = min(min(d0, d1), min(d2, d3));

    float w0 = calc_weight(d0, dm, v0);
    float w1 = calc_weight(d1, dm, v1);
    float w2 = calc_weight(d2, dm, v2);
    float w3 = calc_weight(d3, dm, v3);
    float ws = w0 + w1 + w2 + w3;

    uint2 curr_tex_size = tex_size;

    s_depth[local_index] = dm;
    s_data[local_index] = ws == 0.0 ? (v0 + v1 + v2 + v3) * 0.25 : (v0 * w0 + v1 * w1 + v2 * w2 + v3 * w3) / ws;
    pixel_coord >>= 1;
    curr_tex_size >>= 1;
    if (all(pixel_coord < curr_tex_size)) {
        out_tex_0[pixel_coord] = s_data[local_index];
        out_depth_tex_0[pixel_coord] = dm;
    }
    GroupMemoryBarrierWithGroupSync();

    pixel_coord >>= 1;
    curr_tex_size >>= 1;
    if ((local_index & 3) == 0) {
        float4 v0 = s_data[local_index];
        float4 v1 = s_data[local_index + 1];
        float4 v2 = s_data[local_index + 2];
        float4 v3 = s_data[local_index + 3];
        float d0 = s_depth[local_index];
        float d1 = s_depth[local_index + 1];
        float d2 = s_depth[local_index + 2];
        float d3 = s_depth[local_index + 3];
        float dm = min(min(d0, d1), min(d2, d3));
        s_depth[local_index] = dm;
        float w0 = calc_weight(d0, dm, v0);
        float w1 = calc_weight(d1, dm, v1);
        float w2 = calc_weight(d2, dm, v2);
        float w3 = calc_weight(d3, dm, v3);
        float ws = w0 + w1 + w2 + w3;
        s_data[local_index] = ws == 0.0 ? (v0 + v1 + v2 + v3) * 0.25 : (v0 * w0 + v1 * w1 + v2 * w2 + v3 * w3) / ws;
        if (all(pixel_coord < curr_tex_size)) {
            out_tex_1[pixel_coord] = s_data[local_index];
            out_depth_tex_1[pixel_coord] = dm;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    pixel_coord >>= 1;
    curr_tex_size >>= 1;
    if ((local_index & 15) == 0) {
        float4 v0 = s_data[local_index];
        float4 v1 = s_data[local_index + 4];
        float4 v2 = s_data[local_index + 8];
        float4 v3 = s_data[local_index + 12];
        float d0 = s_depth[local_index];
        float d1 = s_depth[local_index + 4];
        float d2 = s_depth[local_index + 8];
        float d3 = s_depth[local_index + 12];
        float dm = min(min(d0, d1), min(d2, d3));
        s_depth[local_index] = dm;
        float w0 = calc_weight(d0, dm, v0);
        float w1 = calc_weight(d1, dm, v1);
        float w2 = calc_weight(d2, dm, v2);
        float w3 = calc_weight(d3, dm, v3);
        float ws = w0 + w1 + w2 + w3;
        s_data[local_index] = ws == 0.0 ? (v0 + v1 + v2 + v3) * 0.25 : (v0 * w0 + v1 * w1 + v2 * w2 + v3 * w3) / ws;
        if (all(pixel_coord < curr_tex_size)) {
            out_tex_2[pixel_coord] = s_data[local_index];
            out_depth_tex_2[pixel_coord] = dm;
        }
    }
}

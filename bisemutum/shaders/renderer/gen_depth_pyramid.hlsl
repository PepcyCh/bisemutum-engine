#include <bisemutum/shaders/core/utils/bits.hlsl>

#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#if FROM_DEPTH_TEXTURE
#define OUT_BASE 1
#else
#define OUT_BASE 0
#endif

float safe_texel_fetch(int2 pixel_coord) {
#if FROM_DEPTH_TEXTURE
    // return in_depth_tex.Load(int3(clamp(pixel_coord, 0, tex_size - 1), 0)).x;
    return in_depth_tex.SampleLevel(depth_sampler, (pixel_coord + 0.5) / tex_size, 0).x;
#else
    return in_depth_tex[clamp(pixel_coord, 0, tex_size - 1)];
#endif
}

void safe_image_store(RWTexture2D<float> out_tex, float value, uint2 pixel_coord, uint2 tex_size) {
    if (all(pixel_coord < tex_size)) {
        out_tex[pixel_coord] = value;
    }
}

float nearest_depth_of(float d0, float d1, float d2, float d3) {
    return max(max(d0, d1), max(d2, d3));
}

groupshared float s_intermediate[16][16];

[numthreads(256, 1, 1)]
void gen_depth_pyramid_cs(
    uint3 global_thread_id : SV_DispatchThreadID,
    uint3 local_thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID
) {
    const uint2 group_index = uint2(group_id.xy);
    const uint local_index = uint(local_thread_id.x);

    const uint quad_index = WaveGetLaneIndex() & 3;

    const uint local_x = extract_even_bits(local_index);
    const uint local_y = extract_even_bits(local_index >> 1);
    const uint2 pixel_coord_0 = group_index * 64 + uint2(local_x * 2, local_y * 2);

    // level 1

    float d00 = safe_texel_fetch(pixel_coord_0);
    float d01 = safe_texel_fetch(pixel_coord_0 + uint2(1, 0));
    float d02 = safe_texel_fetch(pixel_coord_0 + uint2(0, 1));
    float d03 = safe_texel_fetch(pixel_coord_0 + uint2(1, 1));
    float d0m = nearest_depth_of(d00, d01, d02, d03);

    const uint2 pixel_coord_1 = pixel_coord_0 + uint2(32, 0);
    float d10 = safe_texel_fetch(pixel_coord_1);
    float d11 = safe_texel_fetch(pixel_coord_1 + uint2(1, 0));
    float d12 = safe_texel_fetch(pixel_coord_1 + uint2(0, 1));
    float d13 = safe_texel_fetch(pixel_coord_1 + uint2(1, 1));
    float d1m = nearest_depth_of(d10, d11, d12, d13);

    const uint2 pixel_coord_2 = pixel_coord_0 + uint2(0, 32);
    float d20 = safe_texel_fetch(pixel_coord_2);
    float d21 = safe_texel_fetch(pixel_coord_2 + uint2(1, 0));
    float d22 = safe_texel_fetch(pixel_coord_2 + uint2(0, 1));
    float d23 = safe_texel_fetch(pixel_coord_2 + uint2(1, 1));
    float d2m = nearest_depth_of(d20, d21, d22, d23);

    const uint2 pixel_coord_3 = pixel_coord_0 + uint2(32, 32);
    float d30 = safe_texel_fetch(pixel_coord_3);
    float d31 = safe_texel_fetch(pixel_coord_3 + uint2(1, 0));
    float d32 = safe_texel_fetch(pixel_coord_3 + uint2(0, 1));
    float d33 = safe_texel_fetch(pixel_coord_3 + uint2(1, 1));
    float d3m = nearest_depth_of(d30, d31, d32, d33);

#if FROM_DEPTH_TEXTURE
    safe_image_store(out_depth_tex[0], d00, pixel_coord_0, tex_size);
    safe_image_store(out_depth_tex[0], d01, pixel_coord_0 + uint2(1, 0), tex_size);
    safe_image_store(out_depth_tex[0], d02, pixel_coord_0 + uint2(0, 1), tex_size);
    safe_image_store(out_depth_tex[0], d03, pixel_coord_0 + uint2(1, 1), tex_size);
    safe_image_store(out_depth_tex[0], d10, pixel_coord_1, tex_size);
    safe_image_store(out_depth_tex[0], d11, pixel_coord_1 + uint2(1, 0), tex_size);
    safe_image_store(out_depth_tex[0], d12, pixel_coord_1 + uint2(0, 1), tex_size);
    safe_image_store(out_depth_tex[0], d13, pixel_coord_1 + uint2(1, 1), tex_size);
    safe_image_store(out_depth_tex[0], d20, pixel_coord_2, tex_size);
    safe_image_store(out_depth_tex[0], d21, pixel_coord_2 + uint2(1, 0), tex_size);
    safe_image_store(out_depth_tex[0], d22, pixel_coord_2 + uint2(0, 1), tex_size);
    safe_image_store(out_depth_tex[0], d23, pixel_coord_2 + uint2(1, 1), tex_size);
    safe_image_store(out_depth_tex[0], d30, pixel_coord_3, tex_size);
    safe_image_store(out_depth_tex[0], d31, pixel_coord_3 + uint2(1, 0), tex_size);
    safe_image_store(out_depth_tex[0], d32, pixel_coord_3 + uint2(0, 1), tex_size);
    safe_image_store(out_depth_tex[0], d33, pixel_coord_3 + uint2(1, 1), tex_size);
#endif

    safe_image_store(out_depth_tex[OUT_BASE], d0m, pixel_coord_0 >> 1, tex_size >> 1);
    safe_image_store(out_depth_tex[OUT_BASE], d1m, pixel_coord_1 >> 1, tex_size >> 1);
    safe_image_store(out_depth_tex[OUT_BASE], d2m, pixel_coord_2 >> 1, tex_size >> 1);
    safe_image_store(out_depth_tex[OUT_BASE], d3m, pixel_coord_3 >> 1, tex_size >> 1);

    if (num_levels <= 2) { return; }

    // level 2

    d00 = QuadReadLaneAt(d0m, quad_index);
    d01 = QuadReadLaneAt(d0m, quad_index | 1);
    d02 = QuadReadLaneAt(d0m, quad_index | 2);
    d03 = QuadReadLaneAt(d0m, quad_index | 3);
    d0m = nearest_depth_of(d00, d01, d02, d03);

    d10 = QuadReadLaneAt(d1m, quad_index);
    d11 = QuadReadLaneAt(d1m, quad_index | 1);
    d12 = QuadReadLaneAt(d1m, quad_index | 2);
    d13 = QuadReadLaneAt(d1m, quad_index | 3);
    d1m = nearest_depth_of(d10, d11, d12, d13);

    d20 = QuadReadLaneAt(d2m, quad_index);
    d21 = QuadReadLaneAt(d2m, quad_index | 1);
    d22 = QuadReadLaneAt(d2m, quad_index | 2);
    d23 = QuadReadLaneAt(d2m, quad_index | 3);
    d2m = nearest_depth_of(d20, d21, d22, d23);

    d30 = QuadReadLaneAt(d3m, quad_index);
    d31 = QuadReadLaneAt(d3m, quad_index | 1);
    d32 = QuadReadLaneAt(d3m, quad_index | 2);
    d33 = QuadReadLaneAt(d3m, quad_index | 3);
    d3m = nearest_depth_of(d30, d31, d32, d33);

    if ((local_index & 3) == 0) {
        safe_image_store(out_depth_tex[OUT_BASE + 1], d0m, pixel_coord_0 >> 2, tex_size >> 2);
        safe_image_store(out_depth_tex[OUT_BASE + 1], d1m, pixel_coord_1 >> 2, tex_size >> 2);
        safe_image_store(out_depth_tex[OUT_BASE + 1], d2m, pixel_coord_2 >> 2, tex_size >> 2);
        safe_image_store(out_depth_tex[OUT_BASE + 1], d3m, pixel_coord_3 >> 2, tex_size >> 2);
        s_intermediate[local_x / 2][local_y / 2] = d0m;
        s_intermediate[local_x / 2 + 8][local_y / 2] = d1m;
        s_intermediate[local_x / 2][local_y / 2 + 8] = d2m;
        s_intermediate[local_x / 2 + 8][local_y / 2 + 8] = d3m;
    }

    if (num_levels <= 3) { return; }

    // level 3

    GroupMemoryBarrierWithGroupSync();
    float dt = s_intermediate[local_x][local_y];
    float d0 = QuadReadLaneAt(dt, quad_index);
    float d1 = QuadReadLaneAt(dt, quad_index | 1);
    float d2 = QuadReadLaneAt(dt, quad_index | 2);
    float d3 = QuadReadLaneAt(dt, quad_index | 3);
    float dm = nearest_depth_of(d0, d1, d2, d3);
    if ((local_index & 3) == 0) {
        safe_image_store(out_depth_tex[OUT_BASE + 2], dm, group_index * 8 + uint2(local_x / 2, local_y / 2), tex_size >> 3);
        s_intermediate[local_x][local_y] = dm;
    }

    if (num_levels <= 4) { return; }

    // level 4

    GroupMemoryBarrierWithGroupSync();
    if (local_index < 64) {
        dt = s_intermediate[local_x * 2][local_y * 2];
        d0 = QuadReadLaneAt(dt, quad_index);
        d1 = QuadReadLaneAt(dt, quad_index | 1);
        d2 = QuadReadLaneAt(dt, quad_index | 2);
        d3 = QuadReadLaneAt(dt, quad_index | 3);
        dm = nearest_depth_of(d0, d1, d2, d3);
        if ((local_index & 3) == 0) {
            safe_image_store(out_depth_tex[OUT_BASE + 3], dm, group_index * 4 + uint2(local_x / 2, local_y / 2), tex_size >> 4);
            s_intermediate[local_x * 2][local_y * 2] = dm;
        }
    }

    if (num_levels <= 5) { return; }

    // level 5

    GroupMemoryBarrierWithGroupSync();
    if (local_index < 16) {
        dt = s_intermediate[local_x * 4][local_y * 4];
        d0 = QuadReadLaneAt(dt, quad_index);
        d1 = QuadReadLaneAt(dt, quad_index | 1);
        d2 = QuadReadLaneAt(dt, quad_index | 2);
        d3 = QuadReadLaneAt(dt, quad_index | 3);
        dm = nearest_depth_of(d0, d1, d2, d3);
        if ((local_index & 3) == 0) {
            safe_image_store(out_depth_tex[OUT_BASE + 4], dm, group_index * 2 + uint2(local_x / 2, local_y / 2), tex_size >> 5);
            s_intermediate[local_x * 4][local_y * 4] = dm;
        }
    }

    if (num_levels <= 6) { return; }

    // level 6

    GroupMemoryBarrierWithGroupSync();
    if (local_index < 4) {
        dt = s_intermediate[local_x * 8][local_y * 8];
        d0 = QuadReadLaneAt(dt, quad_index);
        d1 = QuadReadLaneAt(dt, quad_index | 1);
        d2 = QuadReadLaneAt(dt, quad_index | 2);
        d3 = QuadReadLaneAt(dt, quad_index | 3);
        dm = nearest_depth_of(d0, d1, d2, d3);
        if ((local_index & 3) == 0) {
            safe_image_store(out_depth_tex[OUT_BASE + 5], dm, group_index + uint2(local_x / 2, local_y / 2), tex_size >> 6);
        }
    }
}

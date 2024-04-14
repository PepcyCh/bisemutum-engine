#include <bisemutum/shaders/core/utils/depth.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "utils.hlsl"

[numthreads(REBLUR_TILE_SIZE, REBLUR_TILE_SIZE, 1)]
void fetch_linear_depth_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    const int2 pixel_coord = int2(global_thread_id.xy);
    if (any(pixel_coord >= tex_size)) return;

#if REBLUR_USE_HALF_RESOLUTION
    int2 gbuffer_pixel_coord = pixel_coord * 2 + int2(frame_index & 1, (frame_index >> 1) & 1);
    float depth = in_tex.Load(int3(gbuffer_pixel_coord, 0)).x;
#else
    float depth = in_tex.Load(int3(pixel_coord, 0)).x;
#endif
    depth = get_linear_01_depth(depth, matrix_inv_proj);

    out_tex[pixel_coord] = depth;
}

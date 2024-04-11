#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#include "validate_history_flags.hlsl"

[numthreads(16, 16, 1)]
void validate_history_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    uint2 pixel_coord = global_thread_id.xy;
    if (any(pixel_coord >= tex_size)) { return; }
    float2 texcoord = (pixel_coord + 0.5) / tex_size;

    float depth = depth_tex.Load(int3(pixel_coord, 0)).x;
    if (is_depth_background(depth) || has_history == 0) {
        out_validation_tex[pixel_coord] = TEMPORAL_REJECT_BACKGROUND;
        return;
    }

    float2 velocity = velocity_tex.Load(int3(pixel_coord, 0)).xy;
    float2 hist_texcoord = texcoord - velocity;
    float2 prev_pixel_coord_f = hist_texcoord * tex_size;
    int2 prev_pixel_coord = int2(prev_pixel_coord_f);
    if (any(prev_pixel_coord < 0) || any(prev_pixel_coord >= tex_size)) {
        out_validation_tex[pixel_coord] = TEMPORAL_REJECT_MOTION;
        return;
    }

    float hist_depth = hist_depth_tex.Load(int3(prev_pixel_coord, 0)).x;
    if (is_depth_background(hist_depth)) {
        out_validation_tex[pixel_coord] = TEMPORAL_REJECT_PREV_BACKGROUND;
        return;
    }

    uint flags = 0;

    float3 normal = oct_decode(gbuffer_normal.Load(int3(pixel_coord, 0)).xy);

    float3 position_view = position_view_from_depth(texcoord, depth, matrix_inv_proj);
    float3 position_world = mul(matrix_inv_view, float4(position_view, 1.0)).xyz;
    float3 view_vec = camera_position_world() - position_world;
    float camera_dist = length(view_vec);
    float3 view = view_vec / camera_dist;
    float position_radius = min(camera_dist * 0.02 / max(abs(dot(normal, view)), 0.00001), 0.5);

    float3 hist_position_view = position_view_from_depth(hist_texcoord, hist_depth, history_matrix_inv_proj);
    float3 hist_position_world = mul(history_matrix_inv_view, float4(hist_position_view, 1.0)).xyz;
    if (length(position_world - hist_position_world) > position_radius) {
        flags |= TEMPORAL_REJECT_POSITION;
    }

    float3 hist_normal = oct_decode(hist_gbuffer_normal.Load(int3(prev_pixel_coord, 0)).xy);
    if (dot(normal, hist_normal) < 0.9) {
        flags |= TEMPORAL_REJECT_NORMAL;
    }

    out_validation_tex[pixel_coord] = flags;
}

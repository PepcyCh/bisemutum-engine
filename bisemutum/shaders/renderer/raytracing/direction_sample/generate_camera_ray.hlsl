#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

[numthreads(16, 16, 1)]
void generate_camera_ray_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    uint2 pixel_coord = global_thread_id.xy;
    if (any(pixel_coord >= tex_size)) { return; }

    float2 uv = (pixel_coord + 0.5) / tex_size;
    float3 dir_local = normalize((mul(matrix_inv_proj, float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 1.0, 1.0))).xyz);
    float3 dir_world = normalize(mul((float3x3) matrix_inv_view, dir_local));

    ray_origins[pixel_coord] = float4(camera_position_world(), 1.0);
    ray_directions[pixel_coord] = float4(dir_world, 1.0);
    ray_weights[pixel_coord] = 1.0;
}

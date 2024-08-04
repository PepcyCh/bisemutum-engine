#include <bisemutum/shaders/core/shader_params/compute.hlsl>

[numthreads(16, 16, 1)]
void pt_accumulate_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    const int2 pixel_coord = int2(global_thread_id.xy);
    if (any(pixel_coord >= tex_size)) return;
    float3 color = color_tex[pixel_coord].xyz;
    float3 hist_color = history_color_tex.Load(int3(pixel_coord, 0)).xyz;
    color = lerp(hist_color, color, weight);
    color_tex[pixel_coord] = float4(color, 1.0);
}

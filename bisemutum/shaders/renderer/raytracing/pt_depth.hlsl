#include <bisemutum/shaders/core/vertex_attributes.hlsl>
#include <bisemutum/shaders/core/utils/depth.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

float pt_depth_fs(VertexAttributesOutput fin) : SV_Depth {
    int2 pixel_coord = int2(fin.sv_position.xy);
    float4 hit_position = hit_positions.Load(int3(pixel_coord, 0));
    if (hit_position.w < 0.0) {
        return DEVICE_Z_FARTHEST;
    }
    float4 pos_clip = mul(matrix_proj_view, float4(hit_position.xyz, 1.0));
    pos_clip /= pos_clip.w;
    return pos_clip.z;
}

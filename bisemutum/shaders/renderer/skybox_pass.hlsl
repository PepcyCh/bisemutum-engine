#include "../core/vertex_attributes.hlsl"

#include "../core/shader_params/camera.hlsl"
#include "../core/shader_params/fragment.hlsl"

float4 skybox_pass_fs(VertexAttributesOutput fin) : SV_Target {
    float3 local_dir = mul(matrix_inv_proj, float4(fin.position_world, 1.0)).xyz;
    float3 dir = mul((float3x3) matrix_inv_view, local_dir);
    dir = mul((float3x3) skybox_transform, dir);
    return skybox.Sample(skybox_sampler, dir) * float4(skybox_color, 1.0);
}

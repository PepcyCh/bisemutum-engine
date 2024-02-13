#include "../core/vertex_attributes.hlsl"

#include "../core/shader_params/camera.hlsl"
#include "../core/shader_params/material.hlsl"
#include "../core/shader_params/fragment.hlsl"

float4 post_process_pass_fs(VertexAttributesOutput fin) : SV_Target {
    float4 color = input_color.SampleLevel(sampler_input, fin.texcoord0, 0);
    return color;
}

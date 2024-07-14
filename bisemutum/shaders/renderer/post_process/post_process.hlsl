#include <bisemutum/shaders/core/vertex_attributes.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/material.hlsl>
#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

float4 post_process_pass_fs(VertexAttributesOutput fin) : SV_Target {
    float4 color = input_color.SampleLevel(sampler_input, fin.texcoord, 0);
    return float4(color.xyz, 1.0);
}

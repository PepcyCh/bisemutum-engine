#include <bisemutum/shaders/core/vertex_attributes.hlsl>

#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

float4 merge_reflection_fs(VertexAttributesOutput fin) : SV_Target {
    float4 color = color_tex.Sample(input_sampler, fin.texcoord);
    float3 reflection_value = reflection_tex.Sample(input_sampler, fin.texcoord).xyz;
    return float4(color.xyz + reflection_value, color.w);
}

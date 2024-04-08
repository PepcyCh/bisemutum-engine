#include <bisemutum/shaders/core/vertex_attributes.hlsl>

#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

float4 merge_ao_fs(VertexAttributesOutput fin) : SV_Target {
    float4 color = color_tex.Sample(input_sampler, fin.texcoord);
    float ao_value = ao_tex.Sample(input_sampler, fin.texcoord).x;
    color.xyz *= ao_value;
    return color;
}

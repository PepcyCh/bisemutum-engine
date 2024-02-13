#include "../../core/vertex_attributes.hlsl"

#include "../../core/shader_params/fragment.hlsl"

float4 merge_ao_fs(VertexAttributesOutput fin) : SV_Target {
    float4 color = color_tex.Sample(input_sampler, fin.texcoord0);
    float ao_value = ao_tex.Sample(input_sampler, fin.texcoord0).x;
    color.xyz *= ao_value * ao_strength;
    return color;
}

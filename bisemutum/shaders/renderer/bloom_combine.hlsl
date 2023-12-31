#include "../core/vertex_attributes.hlsl"

#include "../core/shader_params/fragment.hlsl"

float4 bloom_combine_fs(VertexAttributesOutput fin) : SV_Target {
    float3 color1 = input_color1.SampleLevel(sampler_input, fin.texcoord0, 0).xyz;
    float3 color2 = input_color2.SampleLevel(sampler_input, fin.texcoord0, 0).xyz;
    return float4(color1 + color2, 1.0);
}

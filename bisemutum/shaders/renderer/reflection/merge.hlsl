#include "../../core/vertex_attributes.hlsl"

#include "../../core/shader_params/fragment.hlsl"

float4 merge_reflection_fs(VertexAttributesOutput fin) : SV_Target {
    float4 color = color_tex.Sample(input_sampler, fin.texcoord);
    float reflection_value = reflection_tex.Sample(input_sampler, fin.texcoord).x;
    return color + reflection_value;
}

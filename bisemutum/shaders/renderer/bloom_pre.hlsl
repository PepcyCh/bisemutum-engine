#include "../core/vertex_attributes.hlsl"
#include "../core/math.hlsl"

#include "../core/shader_params/fragment.hlsl"

float4 bloom_pre_fs(VertexAttributesOutput fin) : SV_Target {
    float3 color = input_color.SampleLevel(sampler_input, fin.texcoord0, 0).xyz;
    float lum = luminance(color);
    float soft = lum + bloom_weight.y;
    soft = clamp(soft, 0.0, bloom_weight.x);
    soft = soft * soft * bloom_weight.w;
    float weight = max(soft, lum - bloom_weight.x) / max(lum, 0.0001);
    return float4(color * weight, 1.0);
}

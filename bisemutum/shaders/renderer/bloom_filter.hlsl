#include "../core/vertex_attributes.hlsl"
#include "../core/math.hlsl"

#include "../core/shader_params/fragment.hlsl"

static const float offsets[] = {
    -3.23076923, -1.38461538, 0.0, 1.38461538, 3.23076923,
};

static const float weights[] = {
    0.07027027, 0.31621622, 0.22702703, 0.31621622, 0.07027027,
};

float4 bloom_horizontal_fs(VertexAttributesOutput fin) : SV_Target {
    float3 sum = 0.0;
    for (int i = 0; i < 5; i++) {
        float offset = offsets[i] * texel_size.x;
        float3 color = input_color.SampleLevel(sampler_input, fin.texcoord + float2(offset, 0.0), 0).xyz;
        sum += color * weights[i];
    }
    return float4(sum, 1.0);
}

float4 bloom_vertical_fs(VertexAttributesOutput fin) : SV_Target {
    float3 sum = 0.0;
    for (int i = 0; i < 5; i++) {
        float offset = offsets[i] * texel_size.y;
        float3 color = input_color.SampleLevel(sampler_input, fin.texcoord + float2(0.0, offset), 0).xyz;
        sum += color * weights[i];
    }
    return float4(sum, 1.0);
}

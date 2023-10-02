#include "../core/vertex_attributes_defines.hlsl"

[[vk::binding(0, 0)]]
Texture2D texture : register(t0, space0);
[[vk::binding(0, 1)]]
SamplerState sampler_texture : register(s0, space1);

struct ImGuiPushConstant {
    float2 scale;
    float2 translate;
};
[[vk::push_constant]]
ConstantBuffer<ImGuiPushConstant> push_c : register(b1, space0);

struct VertexAttributes {
    INPUT_VERTEX_POSITION(float2 pos);
    INPUT_VERTEX_TEXCOORD0(float2 uv);
    INPUT_VERTEX_COLOR(float4 color);
};

struct Vertex2Fragment {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};

Vertex2Fragment imgui_vs(VertexAttributes vin) {
    Vertex2Fragment vout;
    vout.uv = vin.uv;
    vout.color = vin.color;
    float2 pos = vin.pos * push_c.scale + push_c.translate;
    vout.pos = float4(pos.x, -pos.y, 0.0, 1.0);
    return vout;
}

float4 imgui_fs(Vertex2Fragment fin) : SV_Target {
    return fin.color * texture.SampleLevel(sampler_texture, fin.uv, 0);
}

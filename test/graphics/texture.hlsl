struct VertexIn {
    [[vk::location(0)]] float2 pos : POSITION;
    [[vk::location(5)]] float2 uv : TEXCOORD0;
};

struct Vertex2Fragment {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Vertex2Fragment VS(VertexIn vin) {
    Vertex2Fragment vout;
    vout.pos = float4(vin.pos, 0.0, 1.0);
    vout.uv = vin.uv;
    return vout;
}

[[vk::binding(0, 0)]]
Texture2D tex : register(t0, space0);
[[vk::binding(0, 1)]]
SamplerState samp : register(s0, space1);

float4 FS(Vertex2Fragment fin) : SV_TARGET {
    return tex.Sample(samp, fin.uv);
}

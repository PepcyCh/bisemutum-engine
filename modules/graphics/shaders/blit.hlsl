[[vk::binding(0, 0)]]
Texture2D src_texture : register(t0, space0);
[[vk::binding(0, 1)]]
SamplerState samp : register(s1, space1);

struct Vertex2Fragment {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

Vertex2Fragment VS(uint vertex_id : SV_VertexID) {
    Vertex2Fragment vout;
    vout.uv = float2((vertex_id & 1) << 1, vertex_id & 2);
    vout.pos = float4(vout.uv * 2.0f - 1.0f, 0.0, 1.0);
    return vout;
}

#ifndef BLIT_DEPTH
float4 FS(Vertex2Fragment fin) : SV_Target {
#else
float FS(Vertex2Fragment fin) : SV_Depth {
#endif
    return src_texture.SampleLevel(samp, fin.uv, 0);
}

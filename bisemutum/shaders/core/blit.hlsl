[[vk::binding(0, 0)]]
Texture2D src_texture : register(t0, space0);
[[vk::binding(0, 1)]]
SamplerState sampler_src_texture : register(s0, space1);

struct Vertex2Fragment {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

Vertex2Fragment blit_vs(uint vertex_id : SV_VertexID) {
    Vertex2Fragment vout;
    vout.uv = float2((vertex_id & 1) << 1, vertex_id & 2);
    vout.pos = float4(vout.uv.x * 2.0 - 1.0, 1.0 - vout.uv.y * 2.0, 0.0, 1.0);
    return vout;
}

#ifndef BLIT_DEPTH
float4 blit_fs(Vertex2Fragment fin) : SV_Target {
    return src_texture.SampleLevel(sampler_src_texture, fin.uv, 0);
}
#else
float blit_fs(Vertex2Fragment fin) : SV_Depth {
    return src_texture.SampleLevel(sampler_src_texture, fin.uv, 0).x;
}
#endif

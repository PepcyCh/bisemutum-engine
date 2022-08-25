struct VertexIn {
    float2 pos : POSITION;
    float3 color : COLOR;
};

struct Vertex2Fragment {
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

Vertex2Fragment VS(VertexIn vin) {
    Vertex2Fragment vout;
    vout.pos = float4(vin.pos, 0.0, 1.0);
    vout.color = vin.color;
    return vout;
}

float4 FS(Vertex2Fragment fin) : SV_TARGET {
    return float4(fin.color, 1.0);
}

struct VertexIn {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(2)]] float3 normal : NORMAL;
    uint instance_id : SV_InstanceID;
};

struct Vertex2Fragment {
    float4 pos : SV_POSITION;
    float4 world_pos : POSITION;
    float3 normal : NORMAL;
    uint instance_id : TEXCOORD0;
};

struct FragmentOut {
    float3 position : SV_Target0;
    float3 normal : SV_Target1;
    float3 color : SV_Target2;
};

struct Camera {
    float4x4 proj_view;
};
[[vk::binding(1, 0)]]
ConstantBuffer<Camera> camera : register(b1, space0);

struct InstanceData {
    float4x4 model;
    float4x4 model_it;
    float4 color;
};
[[vk::binding(2, 0)]]
StructuredBuffer<InstanceData> instance_data : register(t2, space0);

Vertex2Fragment VS(VertexIn vin) {
    Vertex2Fragment vout;
    vout.world_pos = mul(instance_data[vin.instance_id].model, float4(vin.pos, 1.0));
    vout.pos = mul(camera.proj_view, vout.world_pos);
    vout.normal = mul(instance_data[vin.instance_id].model_it, float4(vin.normal, 0.0));
    vout.instance_id = vin.instance_id;
    return vout;
}

FragmentOut FS(Vertex2Fragment fin) {
    FragmentOut fout;
    fout.position = fin.world_pos.xyz;
    fout.normal = fin.normal;
    InstanceData data = instance_data[fin.instance_id];
    fout.color = data.color;
    return fout;
};

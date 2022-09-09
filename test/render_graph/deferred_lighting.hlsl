[[vk::binding(1, 0)]]
Texture2D gbuffer_position : register(t1, space0);
[[vk::binding(2, 0)]]
Texture2D gbuffer_normal : register(t2, space0);
[[vk::binding(3, 0)]]
Texture2D gbuffer_color : register(t3, space0);
// [[vk::binding(4, 0)]]
// RWTexture2D<float4> output : register(u4, space0);

static const float4 kClearColor = float4(0.2f, 0.3f, 0.5f, 1.0f);

struct PointLight {
    float3 position;
    float strength;
    float3 color;
    float _padding;
};
[[vk::binding(0, 1)]]
ConstantBuffer<PointLight> light : register(b0, space1);

struct Vertex2Fragment {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Vertex2Fragment VS(uint vertex_id : SV_VertexID) {
    Vertex2Fragment vout;
    vout.uv = float2((vertex_id & 1) << 1, vertex_id & 2);
    vout.pos = float4(vout.uv * 2.0f - 1.0f, 0.0, 1.0);
    return vout;
}

float4 FS(Vertex2Fragment fin) : SV_Target {
    uint width, height;
    gbuffer_color.GetDimensions(width, height);
    const uint2 pixel_coord = fin.uv * float2(width, height);

    float3 world_pos = gbuffer_position[pixel_coord].xyz;
    float3 world_normal = gbuffer_normal[pixel_coord].xyz;
    float3 color = gbuffer_color[pixel_coord].xyz;
    if (dot(world_normal, world_normal) < 0.5) {
        return kClearColor;
    }

    float3 light_vec = light.position - world_pos;
    float3 light_dist2 = dot(light_vec, light_vec);
    float3 light_dir = light_vec / sqrt(light_dist2);

    float ndotl = dot(world_normal, light_dir);
    float3 result = (max(ndotl, 0.0) * light.color * light.strength + 0.01) * color;
    return float4(result, 1.0);
}

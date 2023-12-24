#include "math.hlsl"
#include "utils/cubemap.hlsl"

struct EquitangularToCubemapPushConstant {
    uint tex_size;
    float inv_tex_size;
};
[[vk::push_constant]]
ConstantBuffer<EquitangularToCubemapPushConstant> push_c : register(b0, space0);

[[vk::binding(1, 0)]]
Texture2D src_texture : register(t1, space0);
[[vk::binding(2, 0)]]
RWTexture2DArray<float4> dst_cubemap : register(u2, space0);

[[vk::binding(0, 1)]]
SamplerState sampler_src_texture : register(s0, space1);

[numthreads(16, 16, 1)]
void equitangular_to_cubemap_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    if (any(global_thread_id.xy >= push_c.tex_size)) { return; }

    float2 xy = (float2(global_thread_id.xy) + 0.5) * push_c.inv_tex_size;
    float3 dir = cubemap_direction_from_layered_uv(xy, global_thread_id.z);

    float theta = acos(dir.y);
    float phi = dir.z == 0.0 ? 0.0 : atan2(dir.x, dir.z);
    if (phi < 0.0) {
        phi += TWO_PI;
    }

    float2 uv = float2(phi * INV_TWO_PI, theta * INV_PI);
    float4 color = src_texture.SampleLevel(sampler_src_texture, uv, 0.0f);
    dst_cubemap[global_thread_id] = color;
}

struct MipmapPushConstant {
    uint2 tex_size;
};
[[vk::push_constant]]
ConstantBuffer<MipmapPushConstant> push_c;

[[vk::binding(1, 0)]]
RWTexture2D<float4> output : register(u1, space0);

[numthreads(16, 16, 1)]
void CS(int3 dtid : SV_DispatchThreadID) {
    const uint2 pixel_coord = dtid.xy;
    if (any(pixel_coord >= push_c.tex_size)) {
        return;
    }

    float2 z = (float2(pixel_coord) / push_c.tex_size - 0.5) * 2.0;
    const float2 c = float2(-0.75, 0.11);
    const float r = 2.0;
    int iter = 0;
    const int max_iter = 512;
    while (dot(z, z) < r * r && iter < max_iter) {
        z = float2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        ++iter;
    }
    float ratio = float(iter) / max_iter;
    const float pi = 3.14159265359;
    output[pixel_coord] =
        float4(cos(pi * ratio) + 1.0, cos(pi * (ratio + 0.5)) + 1.0, cos(pi * (ratio + 1.0)) + 1.0, 1.0);
}

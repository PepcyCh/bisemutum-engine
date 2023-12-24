struct MipmapPushConstant {
    uint2 tex_size;
};
[[vk::push_constant]]
ConstantBuffer<MipmapPushConstant> push_c : register(b0, space0);

[[vk::binding(1, 0)]]
Texture2D src_texture : register(t1, space0);
#ifdef USE_CS
[[vk::binding(2, 0)]]
RWTexture2D<float4> dst_texture : register(u2, space0);
#endif

#ifndef USE_CS
float4 mipmap_vs(uint vertex_id : SV_VertexID) : SV_POSITION {
    float2 uv = float2((vertex_id & 1) << 1, vertex_id & 2);
    return float4(uv * 2.0 - 1.0, 0.0, 1.0);
}
#endif

#define MIPMAP_MODE_AVG 0
#define MIPMAP_MODE_MIN 1
#define MIPMAP_MODE_MAX 2
#ifndef MIPMAP_MODE
#define MIPMAP_MODE MIPMAP_MODE_AVG
#endif

inline float4 apply_mipmap_op(float4 a, float4 b) {
#if MIPMAP_MODE == MIPMAP_MODE_AVG
    return a + b;
#elif MIPMAP_MODE == MIPMAP_MODE_MIN
    return min(a, b);
#else
    return max(a, b);
#endif
}

#ifndef USE_CS
#ifndef MIPMAP_DEPTH
float4 mipmap_fs(float4 pos : SV_Position) : SV_Target {
#else
float mipmap_fs(float4 pos : SV_Position) : SV_Depth {
#endif
    const uint2 pixel_coord = pos.xy;
#else
[numthreads(16, 16, 1)]
void mipmap_cs(uint3 dtid : SV_DispatchThreadID) {
    const uint2 pixel_coord = dtid.xy;
    if (any(pixel_coord >= push_c.tex_size)) { return; }
#endif
    const uint2 pixel_coord_src = pixel_coord * 2;

    float4 value00 = src_texture[pixel_coord_src];
    float4 value01 = src_texture[pixel_coord_src + uint2(0, 1)];
    float4 value10 = src_texture[pixel_coord_src + uint2(1, 0)];
    float4 value11 = src_texture[pixel_coord_src + uint2(1, 1)];

    float4 result = apply_mipmap_op(apply_mipmap_op(value00, value01), apply_mipmap_op(value10, value11));
#if MIPMAP_MODE == MIPMAP_MODE_AVG
    uint num_pixels = 4;
#endif

    const bool2 is_odd = (push_c.tex_size & 1) != 0;
    if (is_odd.x) {
        float4 value20 = src_texture[pixel_coord_src + uint2(2, 0)];
        float4 value21 = src_texture[pixel_coord_src + uint2(2, 1)];
        result = apply_mipmap_op(result, apply_mipmap_op(value20, value21));
#if MIPMAP_MODE == MIPMAP_MODE_AVG
        num_pixels += 2;
#endif
    }
    if (is_odd.y) {
        float4 value02 = src_texture[pixel_coord_src + uint2(0, 2)];
        float4 value12 = src_texture[pixel_coord_src + uint2(1, 2)];
        result = apply_mipmap_op(result, apply_mipmap_op(value02, value12));
#if MIPMAP_MODE == MIPMAP_MODE_AVG
        num_pixels += 2;
#endif
    }
    if (is_odd.x && is_odd.y) {
        float4 value22 = src_texture[pixel_coord_src + uint2(2, 2)];
        result = apply_mipmap_op(result, value22);
#if MIPMAP_MODE == MIPMAP_MODE_AVG
        num_pixels += 1;
#endif
    }

#if MIPMAP_MODE == MIPMAP_MODE_AVG
    result /= num_pixels;
#endif

#ifndef USE_CS
    return result;
#else
    dst_texture[pixel_coord] = result;
#endif
}

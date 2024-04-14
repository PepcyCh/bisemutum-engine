#pragma once

#include "frame.hlsl"
#include "../utils/math.hlsl"

float2 pack_u32_to_unorm16x2(uint x) {
    return float2((x & 0xffffu) / 65536.0, (x >> 16) / 65536.0);
}
uint unpack_u32_from_unorm16x2(float2 x) {
    uint2 ux = (uint2) (x * 65535.5);
    return ux.x | (ux.y << 16);
}

uint pack_color_rg11b10(float3 color) {
    return uint(color.x * 0x7ff) | (uint(color.y * 0x7ff) << 11) | (uint(color.z * 0x3ff) << 22);
}
float3 unpack_color_rg11b10(uint packed) {
    return float3(
        float(packed & 0x7ff) / 0x7ff,
        float((packed >> 11) & 0x7ff) / 0x7ff,
        float((packed >> 22) & 0x3ff) / 0x3ff
    );
}

uint pack_color_rg11b10_with_max(float3 color, out float max_value) {
    max_value = max(color.x, max(color.y, color.z));
    return max_value == 0.0 ? 0u : pack_color_rg11b10(color / max_value);
}
float3 unpack_color_rg11b10_with_max(uint packed, float max_value) {
    return float3(
        float(packed & 0x7ff) / 0x7ff,
        float((packed >> 11) & 0x7ff) / 0x7ff,
        float((packed >> 22) & 0x3ff) / 0x3ff
    ) * max_value;
}

uint pack_color_rg7b6(float3 color) {
    return uint(color.x * 0x7f) | (uint(color.y * 0x7f) << 7) | (uint(color.z * 0x3f) << 14);
}
float3 unpack_color_rg7b6(uint packed) {
    return float3(
        float(packed & 0x7f) / 0x7f,
        float((packed >> 7) & 0x7f) / 0x7f,
        float((packed >> 14) & 0x3f) / 0x3f
    );
}

uint pack_color_rg7b6_with_unorm12(float3 color, float fp) {
    return uint(color.x * 0x7f) | (uint(color.y * 0x7f) << 7) | (uint(color.z * 0x3f) << 14) | (uint(fp * 0xfff) << 20);
}
float4 unpack_color_rg7b6_with_unorm12(uint packed) {
    return float4(
        float(packed & 0x7f) / 0x7f,
        float((packed >> 7) & 0x7f) / 0x7f,
        float((packed >> 14) & 0x3f) / 0x3f,
        float(packed >> 20) / 0xfff
    );
}

float fp_exp(int e) {
    return e > 0 ? (1u << e) : (1.0 / (1u << -e));
}
uint pack_color_rgb9e5(float3 color) {
    float max_elem = max(color.x, max(color.y, color.z));
    uint max_elem_u32 = asuint(max_elem);
    uint e = uint(clamp(int((max_elem_u32 >> 23) & 0xff) - 127 + 16, 0, 31));
    uint max_m = uint(max_elem * 0x1ff / fp_exp(int(e) - 15));
    e += max_m == 0x1ff && e != 31 ? 1 : 0;
    float ee = fp_exp(int(e) - 15);
    uint rm = uint(color.x / ee * 0x1ff) & 0x1ff;
    uint gm = uint(color.y / ee * 0x1ff) & 0x1ff;
    uint bm = uint(color.z / ee * 0x1ff) & 0x1ff;
    return rm | (gm << 9) | (bm << 18) | (e << 27);
}
float3 unpack_color_rgb9e5(uint packed) {
    uint e = packed >> 27;
    float rm = float(packed & 0x1ff) / 0x1ff;
    float gm = float((packed >> 9) & 0x1ff) / 0x1ff;
    float bm = float((packed >> 18) & 0x1ff) / 0x1ff;
    return float3(rm, gm, bm) * fp_exp(int(e) - 15);
}

float2 oct_wrap(float2 v) {
    return (1.0 - abs(float2(v.yx))) * float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}
float2 oct_encode(float3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : oct_wrap(n.xy);
    return n.xy;
}
float2 oct_encode_01(float3 n) {
    return oct_encode(n) * 0.5 + 0.5;
}
float3 oct_decode(float2 f) {
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.xy = float2(n.xy) + float2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
    return normalize(n);
}
float3 oct_decode_01(float2 f) {
    return oct_decode(f * 2.0 - 1.0);
}
uint oct_encode_u32(float3 n) {
    float2 encoded = oct_encode_01(n);
    return (uint(encoded.x * 65535.0) & 0xffffu) | ((uint(encoded.y * 65535.0) & 0xffffu) << 16);
}
float3 oct_decode_u32(uint f) {
    float2 encoded = float2((f & 0xffffu) / 65535.0, (f >> 16) / 65535.0);
    return oct_decode_01(encoded);
}

float3 pack_normal_and_tangent(float3 N, float3 T) {
    float2 oct_encoded = oct_encode(N);
    Frame frame = create_frame(N);
    float2 projected = float2(dot(T, frame.x), dot(T, frame.y));
    float lnorm = abs(projected.x) + abs(projected.y);
    projected /= lnorm;
    float projected_x = projected.x * 0.5 + 0.5;
    return float3(oct_encoded, projected.y < 0 ? -projected_x : projected_x);
}
void unpack_normal_and_tangent(float3 packed, out float3 N, out float3 T) {
    float2 oct_encoded = packed.xy;
    N = oct_decode(oct_encoded);
    float sign = (packed.z < 0.0 ? -1.0 : 1.0);
    float projected_x = sign * packed.z * 2.0 - 1.0;
    float projected_y = sign * (1.0 - abs(projected_x));
    Frame frame = create_frame(N);
    T = normalize(frame.x * projected_x + frame.y * projected_y);
}

uint pack_normal_and_tangent_u32(float3 N, float3 T) {
    float2 oct_encoded = oct_encode_01(N);
    Frame frame = create_frame(N);
    float2 projected = float2(dot(T, frame.x), dot(T, frame.y));
    float lnorm = abs(projected.x) + abs(projected.y);
    projected /= lnorm;
    return uint(oct_encoded.x * 0x3ff)
        | (uint(oct_encoded.y * 0x3ff) << 10)
        | (uint((projected.x * 0.5 + 0.5) * 0x7ff) << 20)
        | (projected.y < 0 ? 0x80000000u : 0u);
}
uint pack_normal_without_tangent_u32(float3 N) {
    float2 oct_encoded = oct_encode_01(N);
    // `projected` is (1, 0)
    return uint(oct_encoded.x * 0x3ff)
        | (uint(oct_encoded.y * 0x3ff) << 10)
        | (0x7ffu << 20);
}
void unpack_normal_and_tangent_u32(uint packed, out float3 N, out float3 T) {
    float2 oct_encoded = float2(float(packed & 0x3ff) / 0x3ff, float((packed >> 10) & 0x3ff) / 0x3ff);
    N = oct_decode_01(oct_encoded);
    float projected_x = float((packed >> 20) & 0x7ff) / 0x7ff * 2.0 - 1.0;
    float projected_y = ((packed & 0x80000000u) != 0) ? abs(projected_x) - 1.0 : 1.0 - abs(projected_x);
    Frame frame = create_frame(N);
    T = normalize(frame.x * projected_x + frame.y * projected_y);
}

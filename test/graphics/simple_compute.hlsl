[[vk::binding(0, 0)]]
cbuffer Params : register(b0) {
    int2 size;
};

[[vk::binding(1, 0)]]
StructuredBuffer<float4> buf0 : register(t0);
[[vk::binding(2, 0)]]
RWStructuredBuffer<float4> buf1 : register(u0);

float4 Swizzle(float4 v) {
    return v.wzyx;
}

[numthreads(16, 16, 1)]
void SimpleCompute(int2 tid : SV_DispatchThreadID) {
    int index = tid.y * size.x + tid.x;
    buf1[index] = Swizzle(buf0[index]);
}

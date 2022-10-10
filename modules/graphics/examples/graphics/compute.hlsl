struct BlurParams {
    int radius;
    float w0;
    float w1;
    float w2;
    float w3;
    float w4;
    float w5;
    float w6;
    float w7;
    float w8;
    float w9;
    float w10;
};
[[vk::push_constant]]
ConstantBuffer<BlurParams> blur_params;

static const int kMaxBlurRadius = 5;

[[vk::binding(1, 0)]]
Texture2D<float4> input : register(t1, space0);
[[vk::binding(2, 0)]]
RWTexture2D<float4> output : register(u2, space0);

#define N 256
#define kCacheSize (N + kMaxBlurRadius * 2)
groupshared float4 cache[kCacheSize];

[numthreads(N, 1, 1)]
void HoriBlurCS(int3 gtid : SV_GroupThreadID, int3 dtid : SV_DispatchThreadID) {
    int width, height;
    input.GetDimensions(width, height);

    if (gtid.x < blur_params.radius) {
        int x = max(dtid.x - blur_params.radius, 0);
        cache[gtid.x] = input[int2(x, dtid.y)];
    } else if (gtid.x + blur_params.radius >= N) {
        int x = min(dtid.x + blur_params.radius, width - 1);
        cache[gtid.x + 2 * blur_params.radius] = input[int2(x, dtid.y)];
    }
    cache[gtid.x + blur_params.radius] = input[min(dtid.xy, int2(width - 1, height - 1))];

    GroupMemoryBarrierWithGroupSync();

    const float w[] = {
        blur_params.w0,
        blur_params.w1,
        blur_params.w2,
        blur_params.w3,
        blur_params.w4,
        blur_params.w5,
        blur_params.w6,
        blur_params.w7,
        blur_params.w8,
        blur_params.w9,
        blur_params.w10,
    };
    float4 blurred_color = float4(0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < blur_params.radius * 2 + 1; i++) {
        blurred_color += w[i] * cache[gtid.x + i];
    }
    blurred_color.a = 1.0;
    output[dtid.xy] = blurred_color;
}

[numthreads(1, N, 1)]
void VertBlurCS(int3 gtid : SV_GroupThreadID, int3 dtid : SV_DispatchThreadID) {
    int width, height;
    input.GetDimensions(width, height);

    if (gtid.y < blur_params.radius) {
        int y = max(dtid.y - blur_params.radius, 0);
        cache[gtid.y] = input[int2(dtid.x, y)];
    } else if (gtid.y + blur_params.radius >= N) {
        int y = min(dtid.y + blur_params.radius, height - 1);
        cache[gtid.y + 2 * blur_params.radius] = input[int2(dtid.x, y)];
    }
    cache[gtid.y + blur_params.radius] = input[min(dtid.xy, int2(width - 1, height - 1))];

    GroupMemoryBarrierWithGroupSync();

    const float w[] = {
        blur_params.w0,
        blur_params.w1,
        blur_params.w2,
        blur_params.w3,
        blur_params.w4,
        blur_params.w5,
        blur_params.w6,
        blur_params.w7,
        blur_params.w8,
        blur_params.w9,
        blur_params.w10,
    };
    float4 blurred_color = float4(0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < blur_params.radius * 2 + 1; i++) {
        blurred_color += w[i] * cache[gtid.y + i];
    }
    blurred_color.a = 1.0;
    output[dtid.xy] = blurred_color;
}

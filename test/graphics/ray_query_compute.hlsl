[[vk::binding(0, 0)]]
RaytracingAccelerationStructure tlas : register(t0);

[[vk::binding(1, 0)]]
RWTexture2D<float4> output;

struct PushC {
    int2 screen_size;
};
[[vk::push_constant]]
ConstantBuffer<PushC> push_c : register(b0);

[numthreads(16, 16, 1)]
void RayQueryCompute(int2 tid : SV_DispatchThreadID) {
    RayDesc ray;
    ray.Origin = float3(0.0, 0.0, 0.0);
    ray.Direction = normalize(float3((float2(tid) / push_c.screen_size - 0.5) * 2.0, 1.0));
    ray.TMin = 0.001;
    ray.TMax = 1e10;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, ray);
    query.Proceed();
    bool hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;

    output[tid] = hit ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 0.0, 1.0);
}

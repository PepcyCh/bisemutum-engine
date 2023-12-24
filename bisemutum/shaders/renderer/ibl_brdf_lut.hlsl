#include "../core/utils/low_discrepancy.hlsl"
#include "../core/material/utils.hlsl"

#include "../core/shader_params/compute.hlsl"

static const uint num_samples = 1024;

[numthreads(16, 16, 1)]
void ibl_brdf_lut_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    if (any(global_thread_id.xy >= resolution)) { return; }

    float ndotv = (global_thread_id.x + 0.5) * inv_resolution;
    float roughness = (global_thread_id.y + 0.5) * inv_resolution;

    float3 wi = float3(sqrt(1.0 - ndotv * ndotv), 0.0, ndotv);

    float2 value = 0.0;
    for (uint i = 0; i < num_samples; i++) {
        float2 rand2 = hammersley(i, num_samples);
        float3 wh = ggx_vndf_sample(wi, roughness, roughness, rand2);
        float3 wo = reflect(-wi, wh);
        if (wo.z <= 0.0) { continue; }
        float weight = ggx_vndf_sample_weight(wi, wo, roughness, roughness);

        float hdotv = dot(wi, wh);
        float f = pow5(1.0 - hdotv);
        value += float2(1.0 - f, f) * weight;
    }
    value /= num_samples;

    brdf_lut[global_thread_id.xy] = value;
}

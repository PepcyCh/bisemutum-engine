#include <bisemutum/shaders/core/utils/color.hlsl>
#include <bisemutum/shaders/core/utils/frame.hlsl>
#include <bisemutum/shaders/core/utils/low_discrepancy.hlsl>
#include <bisemutum/shaders/core/material/utils.hlsl>
#include <bisemutum/shaders/core/utils/cubemap.hlsl>

#include <bisemutum/shaders/core/shader_params/compute.hlsl>

static const uint num_samples = 1024;
static const float clamp_lum = 12.0;

[numthreads(16, 16, 1)]
void skybox_precompute_specular_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    if (any(global_thread_id.xy >= tex_size)) { return; }

    float2 xy = (float2(global_thread_id.xy) + 0.5) * inv_tex_size;
    float3 dir = cubemap_direction_from_layered_uv(xy, global_thread_id.z);

    Frame frame = create_frame(dir);
    float3 wi = float3(0.0, 0.0, 1.0);

    float3 center_color = skybox.SampleLevel(skybox_sampler, dir, 0.0).xyz;

    float3 filtered = 0.0;
    float weight_sum = 0.0;
    for (uint i = 0; i < num_samples; i++) {
        float2 rand2 = hammersley(i, num_samples);
        float3 wh = ggx_vndf_sample(wi, roughness, roughness, rand2);
        float3 wo = reflect(-wi, wh);
        if (wo.z <= 0.0) { continue; }

        float3 wo_world = frame_to_world(frame, wo);
        float3 color = skybox.SampleLevel(skybox_sampler, wo_world, 0.0).xyz;
        float scale = clamp_lum / max(luminance(color), clamp_lum);
        filtered += color * scale * wo.z;
        weight_sum += wo.z;
    }
    filtered /= weight_sum;

    specular_filtered[global_thread_id] = float4(filtered, 1.0);
}

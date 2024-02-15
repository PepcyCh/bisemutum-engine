#include "../core/math.hlsl"
#include "../core/utils/frame.hlsl"
#include "../core/utils/cubemap.hlsl"

#include "../core/shader_params/compute.hlsl"

static const uint num_samples_sqrt = 64;
static const uint num_samples = num_samples_sqrt * num_samples_sqrt;

[numthreads(16, 16, 1)]
void skybox_precompute_diffuse_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    if (any(global_thread_id.xy >= tex_size)) { return; }

    float2 xy = (float2(global_thread_id.xy) + 0.5) * inv_tex_size;
    float3 dir = cubemap_direction_from_layered_uv(xy, global_thread_id.z);

    float3 irradiance = 0.0;
    Frame frame = create_frame(dir);

    float3 center_color = skybox.SampleLevel(skybox_sampler, dir, 0.0).xyz;
    float clamp_lum = max(luminance(center_color) * 16.0, 0.1);

    float delta = PI / num_samples_sqrt;
    for (float phi = delta * 0.5; phi < TWO_PI; phi += delta) {
        float cos_phi = cos(phi);
        float sin_phi = sin(phi);
        for (float theta = delta * 0.5; theta < 0.5 * PI; theta += delta) {
            float cos_theta = cos(theta);
            float sin_theta = sin(theta);
            float3 v = float3(sin_theta * cos_phi, sin_theta * sin_phi, cos_theta);
            v = frame_to_world(frame, v);
            float3 color = skybox.SampleLevel(skybox_sampler, v, 0.0).xyz;
            float scale = 1.0; // clamp_lum / max(luminance(color), clamp_lum);
            irradiance += color * scale * cos_theta * sin_theta;
        }
    }
    irradiance = PI * irradiance / num_samples;

    diffuse_irradiance[global_thread_id] = float4(irradiance, 1.0);
}

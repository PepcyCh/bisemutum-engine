#include "../core/math.hlsl"
#include "../core/utils/frame.hlsl"

#include "../core/shader_params/compute.hlsl"

[numthreads(16, 16, 1)]
void skybox_precompute_diffuse_cs(uint3 global_thread_id: SV_DispatchThreadID) {
    float2 xy = (float2(global_thread_id.xy) * 2.0 + 1.0) * inv_size - 1.0;
    float3 dir;
    if (global_thread_id.z == 0) { // +X
        dir = normalize(float3(1.0, 1.0 - xy.y, 1.0 - xy.x));
    } else if (global_thread_id.z == 1) { // -X
        dir = normalize(float3(-1.0, xy.y, 1.0 - xy.x));
    } else if (global_thread_id.z == 2) { // +Y
        dir = normalize(float3(xy.x, 1.0, xy.y));
    } else if (global_thread_id.z == 3) { // -Y
        dir = normalize(float3(xy.x, -1.0, 1.0 - xy.y));
    } else if (global_thread_id.z == 4) { // +Z
        dir = normalize(float3(xy.x, 1.0 - xy.y, 1.0));
    } else { // -Z
        dir = normalize(float3(1.0 - xy.x, 1.0 - xy.y, -1.0));
    }

    float3 irradiance = 0.0;
    Frame frame = create_frame(dir);

    float delta = PI / 64.0;
    for (float phi = 0.0; phi < TWO_PI; phi += delta) {
        float cos_phi = cos(phi);
        float sin_phi = sin(phi);
        for (float theta = 0.0; theta < 0.5 * PI; theta += delta) {
            float cos_theta = cos(theta);
            float sin_theta = sin(theta);
            float3 v = float3(sin_theta * cos_phi, sin_theta * sin_phi, cos_theta);
            v = frame_to_world(frame, v);
            float3 color = skybox.SampleLevel(skybox_sampler, v, 0.0).xyz;
            irradiance += color * cos_theta * sin_theta;
        }
    }
    irradiance = PI * irradiance / (64 * 64);

    diffuse_irradiance[global_thread_id] = float4(irradiance, 1.0);
}

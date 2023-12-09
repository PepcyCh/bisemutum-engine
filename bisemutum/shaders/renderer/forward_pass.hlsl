#include "../core/vertex_attributes.hlsl"
#include "lights.hlsl"

#include "../core/shader_params/camera.hlsl"
#include "../core/shader_params/material.hlsl"
#include "../core/shader_params/fragment.hlsl"

float4 forward_pass_fs(VertexAttributesOutput fin) : SV_Target {
    float3 N = normalize(fin.normal_world);
    float3 T = normalize(fin.tangent_world);
    float3 B = normalize(fin.bitangent_world);
    float3 V = normalize(camera_position_world() - fin.position_world);

    SurfaceData surface = material_function(fin);
    float3 normal_tspace = surface.normal_map_value * 2.0 - 1.0;
    N = normalize(normal_tspace.x * T + normal_tspace.y * B + normal_tspace.z * N);

    float3 color = 0.0;
    float3 light_dir;

    int i;
    for (i = 0; i < num_dir_lights; i++) {
        LightData light = dir_lights[i];
        float3 le = dir_light_eval(light, fin.position_world, light_dir);
        float shadow_factor = dir_light_shadow_factor(
            light, fin.position_world, dir_lights_shadow_transform[light.sm_index],
            dir_lights_shadow_map, shadow_map_sampler
        );
        color += le * shadow_factor * surface_eval(N, T, B, V, light_dir, surface);
    }
    for (i = 0; i < num_point_lights; i++) {
        LightData light = point_lights[i];
        float3 le = point_light_eval(light, fin.position_world, light_dir);
        color += le * surface_eval(N, T, B, V, light_dir, surface);
    }
    for (i = 0; i < num_spot_lights; i++) {
        LightData light = spot_lights[i];
        float3 le = spot_light_eval(light, fin.position_world, light_dir);
        color += le * surface_eval(N, T, B, V, light_dir, surface);
    }

    return float4(color, 1.0);
}

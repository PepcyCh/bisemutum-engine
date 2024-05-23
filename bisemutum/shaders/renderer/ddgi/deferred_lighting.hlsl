#include <bisemutum/shaders/core/material.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>

#include "ddgi_struct.hlsl"
#include "../lights.hlsl"
#include "../gbuffer.hlsl"
#include "../skybox/skybox_common.hlsl"

#include <bisemutum/shaders/core/shader_params/compute.hlsl>

[numthreads(32, 8, 1)]
void ddgi_deferred_lighting_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    uint ray_index = global_thread_id.x;
    uint probe_index_linear = global_thread_id.y;
    if (probe_index_linear >= DDGI_PROBES_COUNT || ray_index >= DDGI_NUM_RAYS_PER_PROBE) { return; }
    uint2 output_coord = uint2(ray_index, probe_index_linear);

    uint3 probe_index = uint3(
        probe_index_linear % DDGI_PROBES_SIZE,
        (probe_index_linear / DDGI_PROBES_SIZE) % DDGI_PROBES_SIZE,
        probe_index_linear / DDGI_PROBES_SIZE / DDGI_PROBES_SIZE
    );
    float3 probe_center = volume_base_position
        + probe_index.x * volume_extent_x / (DDGI_PROBES_SIZE - 1) * volume_frame_x
        + probe_index.y * volume_extent_y / (DDGI_PROBES_SIZE - 1) * volume_frame_y
        + probe_index.z * volume_extent_z / (DDGI_PROBES_SIZE - 1) * volume_frame_z;

    float4 hit_position = probe_gbuffer_position.Load(int3(output_coord, 0));
    if (hit_position.w < 0.0) {
        float3 dir = mul((float3x3) skybox_transform, hit_position.xyz);
        float3 color = skybox.SampleLevel(skybox_sampler, dir, 0) * float4(skybox_color, 1.0);
        trace_radiance[output_coord] = float4(color, 1.0);
        return;
    }

    float4 base_color = probe_gbuffer_base_color.Load(int3(output_coord, 0));
    float4 normal_roughness = probe_gbuffer_normal_roughness.Load(int3(output_coord, 0));

    float3 N, T;
    unpack_normal_and_tangent(normal_roughness.xyz, N, T);
    float3 B = cross(N, T);

    SurfaceData surface = surface_data_diffuse(base_color.xyz);
    const uint surface_model = MATERIAL_SURFACE_MODEL_LIT;

    float3 position_world = hit_position.xyz;
    float3 V = normalize(probe_center - position_world);

    float3 color = 0.0;
    float3 light_dir;

    int i;
    for (i = 0; i < num_dir_lights; i++) {
        DirLightData light = dir_lights[i];
        float3 le = dir_light_eval(light, position_world, light_dir);
        float shadow_factor = dir_light_shadow_factor(
            light, position_world, N,
            camera_position_world(),
            dir_lights_shadow_transform, dir_lights_shadow_map, shadow_map_sampler
        );
        color += le * shadow_factor * surface_eval(N, T, B, V, light_dir, surface, surface_model);
    }
    for (i = 0; i < num_point_lights; i++) {
        PointLightData light = point_lights[i];
        float3 le = point_light_eval(light, position_world, light_dir);
        float shadow_factor = point_light_shadow_factor(
            light, position_world, N,
            camera_position_world(),
            point_lights_shadow_transform, point_lights_shadow_map, shadow_map_sampler
        );
        color += le * shadow_factor * surface_eval(N, T, B, V, light_dir, surface, surface_model);
    }

    float roughness_x, roughness_y;
    get_anisotropic_roughness(surface.roughness, surface.anisotropy, roughness_x, roughness_y);
    LtcLuts ltc_luts;
    ltc_luts.matrix_lut0 = ltc_matrix_lut0;
    ltc_luts.matrix_lut1 = ltc_matrix_lut1;
    ltc_luts.matrix_lut2 = ltc_matrix_lut2;
    ltc_luts.norm_lut = ltc_norm_lut;
    ltc_luts.sampler = ltc_sampler;
    for (i = 0; i < num_rect_lights; i++) {
        RectLightData light = rect_lights[i];
        float3 fr = schlick_fresnel(surface.f0_color, surface.f90_color, max(dot(V, N), 0.0), 1.5);
        float3 ltc_spec, ltc_diff;
        float2 ltc_brdf;
        rect_light_eval_ltc(
            rect_light_textures,
            rect_light_samplers,
            ltc_luts,
            position_world, N, T, B, V,
            light,
            roughness_x, roughness_y, surface.f0_color, surface.f90_color,
            ltc_spec, ltc_diff, ltc_brdf
        );
        float3 lighting = surface_eval_lut(N, V, surface, ltc_diff, ltc_spec, ltc_brdf, surface_model);
        color += lighting;
    }

    // TODO: calc DDGI here

    trace_radiance[output_coord] = float4(color, 1.0);
}

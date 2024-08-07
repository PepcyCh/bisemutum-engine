#include <bisemutum/shaders/core/material.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>

#include "../lights.hlsl"
#include "../gbuffer.hlsl"
#include "../skybox/skybox_common.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

[numthreads(16, 16, 1)]
void deferred_lighting_secondary_cs(uint3 global_thread_id : SV_DispatchThreadID) {
    uint2 pixel_coord = global_thread_id.xy;
    if (any(pixel_coord >= tex_size)) { return; }
    float2 texcoord = (pixel_coord + 0.5) / tex_size;

    float3 color_weight = weights.Load(int3(pixel_coord, 0)).xyz * lighting_strength;
    if (all(color_weight == 0.0)) {
        color_tex[pixel_coord] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float4 hit_position = hit_positions.Load(int3(pixel_coord, 0));
    if (hit_position.w < 0.0) {
        float3 dir = mul((float3x3) skybox_transform, hit_position.xyz);
        float3 color = skybox.SampleLevel(skybox_sampler, dir, 0) * float4(skybox_color, 1.0);
        color_tex[pixel_coord] = float4(color * color_weight, 1.0);
        return;
    }

    GBuffer gbuffer;
    gbuffer.base_color = gbuffer_textures[GBUFFER_BASE_COLOR].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.normal_roughness = gbuffer_textures[GBUFFER_NORMAL_ROUGHNESS].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.fresnel = gbuffer_textures[GBUFFER_FRESNEL].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.material_0 = gbuffer_textures[GBUFFER_MATERIAL_0].SampleLevel(gbuffer_sampler, texcoord, 0);

    float3 N, T;
    SurfaceData surface;
    uint surface_model;
    unpack_gbuffer_to_surface(gbuffer, N, T, surface, surface_model);
    float3 B = cross(N, T);

    float3 position_world = hit_position.xyz;
    float3 view_position = view_positions.Load(int3(pixel_coord, 0)).xyz;
    float3 V = normalize(view_position - position_world);

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

#ifndef DEFERRED_LIGHTING_NO_IBL
    float3 skybox_N = mul((float3x3) skybox_transform, N);
    float3 ibl_diffuse = skybox_diffuse_irradiance.SampleLevel(skybox_sampler, skybox_N, 0).xyz * skybox_diffuse_color;
    float3 skybox_R = mul((float3x3) skybox_transform, reflect(-V, N));
    float3 ibl_specular = skybox_specular_filtered.SampleLevel(
        skybox_sampler, skybox_R, surface.roughness * (ibl_specular_num_levels - 1)
    ).xyz * skybox_specular_color;
    float2 ibl_brdf = skybox_brdf_lut.SampleLevel(skybox_sampler, float2(dot(N, V), surface.roughness), 0).xy;
    float3 ibl = surface_eval_lut(N, V, surface, ibl_diffuse, ibl_specular, ibl_brdf, surface_model);
    color += ibl;
#endif

    color_tex[pixel_coord] = float4(color * color_weight, 1.0);
}

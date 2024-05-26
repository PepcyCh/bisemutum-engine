#include <bisemutum/shaders/core/vertex_attributes.hlsl>
#include <bisemutum/shaders/core/material.hlsl>
#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>

#include "gbuffer.hlsl"
#include "lights.hlsl"
#include "skybox/skybox_common.hlsl"
#include "ddgi/ddgi_lighting.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

float4 deferred_lighting_pass_fs(VertexAttributesOutput fin) : SV_Target {
    GBuffer gbuffer;
    gbuffer.base_color = gbuffer_textures[GBUFFER_BASE_COLOR].Sample(gbuffer_sampler, fin.texcoord);
    gbuffer.normal_roughness = gbuffer_textures[GBUFFER_NORMAL_ROUGHNESS].Sample(gbuffer_sampler, fin.texcoord);
    gbuffer.fresnel = gbuffer_textures[GBUFFER_FRESNEL].Sample(gbuffer_sampler, fin.texcoord);
    gbuffer.material_0 = gbuffer_textures[GBUFFER_MATERIAL_0].Sample(gbuffer_sampler, fin.texcoord);
    float depth = depth_texture.Sample(gbuffer_sampler, fin.texcoord).x;
    if (is_depth_background(depth)) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 N, T;
    SurfaceData surface;
    uint surface_model;
    unpack_gbuffer_to_surface(gbuffer, N, T, surface, surface_model);
    float3 B = cross(N, T);

    float3 position_view = position_view_from_depth(fin.texcoord, depth, matrix_inv_proj);
    float3 position_world = mul(matrix_inv_view, float4(position_view, 1.0)).xyz;
    float3 V = normalize(camera_position_world() - position_world);

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

    float3 skybox_N = mul((float3x3) skybox_transform, N);
    float3 ibl_diffuse = skybox_diffuse_irradiance.Sample(skybox_sampler, skybox_N).xyz * skybox_diffuse_color;
    float3 skybox_R = mul((float3x3) skybox_transform, reflect(-V, N));
    float3 ibl_specular = skybox_specular_filtered.SampleLevel(
        skybox_sampler, skybox_R, surface.roughness * (ibl_specular_num_levels - 1)
    ).xyz * skybox_specular_color;
    float2 ibl_brdf = skybox_brdf_lut.Sample(skybox_sampler, float2(dot(N, V), surface.roughness)).xy;
    float3 ibl = surface_eval_lut(N, V, surface, ibl_diffuse, ibl_specular, ibl_brdf, surface_model);
    color += ibl;

    float4 ddgi_color = 0.0;
    for (uint i = 0; i < ddgi_num_volumes; i++) {
        ddgi_color += calc_ddgi_volume_lighting(
            position_world, N, V,
            ddgi_volumes[i],
            i,
            ddgi_irradiance_texture,
            ddgi_visibility_texture,
            ddgi_sampler
        );
    }
    if (ddgi_color.a > 0.0) {
        ddgi_color /= ddgi_color.a;
        color += ddgi_color.xyz * surface.base_color * INV_PI * ddgi_strength;
    }

    return float4(color, 1.0);
}

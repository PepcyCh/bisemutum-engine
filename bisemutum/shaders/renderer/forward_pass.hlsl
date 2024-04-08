#include <bisemutum/shaders/core/vertex_attributes.hlsl>

#include "lights.hlsl"
#include "skybox_common.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/material.hlsl>
#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

struct Output {
    float4 color : SV_Target0;
    float4 velocity : SV_Target1;
};

Output forward_pass_fs(VertexAttributesOutput fin) {
    float3 N = normalize(fin.normal_world);
    float3 T = normalize(fin.tangent_world);
    float3 B = normalize(fin.bitangent_world);
    float3 V = normalize(camera_position_world() - fin.position_world);

    SurfaceData surface = material_function(fin);
    float3 normal_tspace = surface.normal_map_value * 2.0 - 1.0;
    N = normalize(normal_tspace.x * T + normal_tspace.y * B + normal_tspace.z * N);
    B = cross(N, T);

    float3 color = 0.0;
    float3 light_dir;

    int i;
    for (i = 0; i < num_dir_lights; i++) {
        DirLightData light = dir_lights[i];
        float3 le = dir_light_eval(light, fin.position_world, light_dir);
        float shadow_factor = dir_light_shadow_factor(
            light, fin.position_world, N,
            camera_position_world(),
            dir_lights_shadow_transform, dir_lights_shadow_map, shadow_map_sampler
        );
        color += le * shadow_factor * surface_eval(N, T, B, V, light_dir, surface);
    }
    for (i = 0; i < num_point_lights; i++) {
        PointLightData light = point_lights[i];
        float3 le = point_light_eval(light, fin.position_world, light_dir);
        float shadow_factor = point_light_shadow_factor(
            light, fin.position_world, N,
            camera_position_world(),
            point_lights_shadow_transform, point_lights_shadow_map, shadow_map_sampler
        );
        color += le * shadow_factor * surface_eval(N, T, B, V, light_dir, surface);
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
            fin.position_world, N, T, B, V,
            light,
            roughness_x, roughness_y, surface.f0_color, surface.f90_color,
            ltc_spec, ltc_diff, ltc_brdf
        );
        float3 lighting = surface_eval_lut(N, V, surface, ltc_diff, ltc_spec, ltc_brdf);
        color += lighting;
    }

    float3 skybox_N = mul((float3x3) skybox_transform, N);
    float3 ibl_diffuse = skybox_diffuse_irradiance.Sample(skybox_sampler, skybox_N).xyz * skybox_diffuse_color;
    float3 skybox_R = mul((float3x3) skybox_transform, reflect(-V, N));
    float3 ibl_specular = skybox_specular_filtered.SampleLevel(
        skybox_sampler, skybox_R, surface.roughness * (ibl_specular_num_levels - 1)
    ).xyz * skybox_specular_color;
    float2 ibl_brdf = skybox_brdf_lut.Sample(skybox_sampler, float2(dot(N, V), surface.roughness)).xy;
    float3 ibl = surface_eval_lut(N, V, surface, ibl_diffuse, ibl_specular, ibl_brdf);
    color += ibl;

    Output result;
    result.color = float4(color, 1.0);

    float4 history_pos_clip = mul(history_matrix_proj_view, float4(fin.history_position_world, 1.0));
    history_pos_clip.xyz /= history_pos_clip.w;
    float2 history_uv = float2(history_pos_clip.x * 0.5 + 0.5, 0.5 - history_pos_clip.y * 0.5);
    result.velocity = float4(fin.sv_position.xy / viewport_size - history_uv, 0.0, 1.0);

    return result;
}

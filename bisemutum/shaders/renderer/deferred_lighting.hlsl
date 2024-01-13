#include "../core/vertex_attributes.hlsl"

#include "lights.hlsl"
#include "skybox_common.hlsl"

#include "../core/shader_params/camera.hlsl"
#include "../core/shader_params/fragment.hlsl"

#include "../core/material.hlsl"
#include "../core/utils/projection.hlsl"
#include "gbuffer.hlsl"

float4 deferred_lighting_pass_fs(VertexAttributesOutput fin) : SV_Target {
    GBuffer gbuffer;
    gbuffer.base_color = gbuffer_textures[GBUFFER_BASE_COLOR].Sample(gbuffer_sampler, fin.texcoord0);
    gbuffer.normal_roughness = gbuffer_textures[GBUFFER_NORMAL_ROUGHNESS].Sample(gbuffer_sampler, fin.texcoord0);
    gbuffer.specular_model = gbuffer_textures[GBUFFER_SPECULAR_MODEL].Sample(gbuffer_sampler, fin.texcoord0);
    gbuffer.additional_0 = gbuffer_textures[GBUFFER_ADDITIONAL_0].Sample(gbuffer_sampler, fin.texcoord0);
    float depth = depth_texture.Sample(gbuffer_sampler, fin.texcoord0).x;
    if (depth == 1.0) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 N, T;
    SurfaceData surface;
    uint surface_model;
    unpack_gbuffer_to_surface(gbuffer, N, T, surface, surface_model);
    float3 B = cross(N, T);

    float3 position_view = position_view_from_depth(fin.texcoord0, depth, matrix_inv_proj);
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
        LightData light = point_lights[i];
        float3 le = point_light_eval(light, position_world, light_dir);
        color += le * surface_eval(N, T, B, V, light_dir, surface, surface_model);
    }
    for (i = 0; i < num_spot_lights; i++) {
        LightData light = spot_lights[i];
        float3 le = spot_light_eval(light, position_world, light_dir);
        color += le * surface_eval(N, T, B, V, light_dir, surface, surface_model);
    }

    float3 skybox_N = mul((float3x3) skybox_transform, N);
    float3 ibl_diffuse = skybox_diffuse_irradiance.Sample(skybox_sampler, skybox_N).xyz * skybox_diffuse_color;
    float3 skybox_R = mul((float3x3) skybox_transform, reflect(-V, N));
    float3 ibl_specular = skybox_specular_filtered.SampleLevel(
        skybox_sampler, skybox_R, surface.roughness * (ibl_specular_num_levels - 1)
    ).xyz * skybox_specular_color;
    color += skybox_diffuse_irradiance.Sample(skybox_sampler, N) * skybox_diffuse_color * ibl_diffuse;
    float2 ibl_brdf = skybox_brdf_lut.Sample(skybox_sampler, float2(dot(N, V), surface.roughness)).xy;
    float3 ibl = surface_eval_ibl(N, V, surface, ibl_diffuse, ibl_specular, ibl_brdf, surface_model);
    color += ibl;

    return float4(color, 1.0);
}

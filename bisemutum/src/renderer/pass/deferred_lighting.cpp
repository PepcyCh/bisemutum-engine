#include "deferred_lighting.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(DeferredLightingPassParams)
    BI_SHADER_PARAMETER(uint, num_dir_lights)
    BI_SHADER_PARAMETER(uint, num_point_lights)
    BI_SHADER_PARAMETER(float3, skybox_diffuse_color)
    BI_SHADER_PARAMETER(float3, skybox_specular_color)
    BI_SHADER_PARAMETER(float4x4, skybox_transform)

    BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(Texture2D, gbuffer_textures, [GBufferTextures::count])
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_texture)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, gbuffer_sampler)

    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<DirLightData>, dir_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<PointLightData>, point_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float4x4>, dir_lights_shadow_transform)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2DArray, dir_lights_shadow_map)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, shadow_map_sampler)

    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox_diffuse_irradiance)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox_specular_filtered)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, skybox_brdf_lut)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, skybox_sampler)
BI_SHADER_PARAMETERS_END(DeferredLightingPassParams)

struct PassData final {
    gfx::TextureHandle output;
    gfx::TextureHandle depth;
    GBufferTextures gbuffer;
};

}

DeferredLightingPass::DeferredLightingPass() {
    fragment_shader_params.initialize<DeferredLightingPassParams>();

    fragment_shader.source_path = "/bisemutum/shaders/renderer/deferred_lighting.hlsl";
    fragment_shader.source_entry = "deferred_lighting_pass_fs";
    fragment_shader.set_shader_params_struct<DeferredLightingPassParams>();
    fragment_shader.depth_test = false;
    fragment_shader.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
}

auto DeferredLightingPass::update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void {
    auto& current_skybox = g_engine->system_manager()->get_system_for_current_scene<SkyboxSystem>()->current_skybox();

    auto params = fragment_shader_params.mutable_typed_data<DeferredLightingPassParams>();
    params->num_dir_lights = lights_ctx.dir_lights.size();
    params->num_point_lights = lights_ctx.point_lights.size();
    params->skybox_diffuse_color = current_skybox.color * current_skybox.diffuse_strength;
    params->skybox_specular_color = current_skybox.color * current_skybox.specular_strength;
    params->skybox_transform = current_skybox.transform;

    params->dir_lights = {&lights_ctx.dir_lights_buffer, 0};
    params->point_lights = {&lights_ctx.point_lights_buffer, 0};
    params->dir_lights_shadow_transform = {&lights_ctx.dir_lights_shadow_transform_buffer, 0};
    params->dir_lights_shadow_map = {&lights_ctx.dir_lights_shadow_map};
    params->shadow_map_sampler = {lights_ctx.shadow_map_sampler};

    params->skybox_diffuse_irradiance = {&skybox_ctx.diffuse_irradiance};
    params->skybox_specular_filtered = {&skybox_ctx.specular_filtered};
    params->skybox_brdf_lut = {&skybox_ctx.brdf_lut};
    params->skybox_sampler = {skybox_ctx.skybox_sampler};
}

auto DeferredLightingPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData {
    auto& camera_target = camera.target_texture();

    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("Deferred Lighting Pass");

    pass_data->depth = builder.read(input.depth);
    pass_data->gbuffer = input.gbuffer.read(builder);
    pass_data->output = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::rgba16_sfloat,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });

    builder.use_color(
        0,
        gfx::GraphicsPassColorTargetBuilder{pass_data->output}.clear_color()
    );

    builder.read(input.dir_lighst_shadow_map);

    builder.read(input.skybox.skybox);
    builder.read(input.skybox.diffuse_irradiance);
    builder.read(input.skybox.specular_filtered);
    builder.read(input.skybox.brdf_lut);

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = fragment_shader_params.mutable_typed_data<DeferredLightingPassParams>();
            params->gbuffer_textures[0] = {ctx.rg->texture(pass_data->gbuffer.base_color)};
            params->gbuffer_textures[1] = {ctx.rg->texture(pass_data->gbuffer.normal_roughness)};
            params->gbuffer_textures[2] = {ctx.rg->texture(pass_data->gbuffer.fresnel)};
            params->gbuffer_textures[3] = {ctx.rg->texture(pass_data->gbuffer.material_0)};
            params->depth_texture = {ctx.rg->texture(pass_data->depth)};
            params->gbuffer_sampler = {pass_data->gbuffer.get_sampler()};
            fragment_shader_params.update_uniform_buffer();
            ctx.render_full_screen(camera, fragment_shader, fragment_shader_params);
        }
    );

    return OutputData{
        .output = pass_data->output,
    };
}

}

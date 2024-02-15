#include "deferred_lighting.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(DeferredLightingPassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(Texture2D, gbuffer_textures, [GBufferTextures::count])
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_texture)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, gbuffer_sampler)

    BI_SHADER_PARAMETER_INCLUDE(LightsContextShaderData, lights_ctx)
    BI_SHADER_PARAMETER_INCLUDE(SkyboxContextShaderData, skybox_ctx)
BI_SHADER_PARAMETERS_END(DeferredLightingPassParams)

struct PassData final {
    gfx::TextureHandle output;
    gfx::TextureHandle depth;
    GBufferTextures gbuffer;
};

}

DeferredLightingPass::DeferredLightingPass() {
    fragment_shader_params_.initialize<DeferredLightingPassParams>();

    fragment_shader_.source_path = "/bisemutum/shaders/renderer/deferred_lighting.hlsl";
    fragment_shader_.source_entry = "deferred_lighting_pass_fs";
    fragment_shader_.set_shader_params_struct<DeferredLightingPassParams>();
    fragment_shader_.depth_test = false;
    fragment_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
}

auto DeferredLightingPass::update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void {
    auto params = fragment_shader_params_.mutable_typed_data<DeferredLightingPassParams>();

    params->lights_ctx = lights_ctx.shader_data;
    params->skybox_ctx = skybox_ctx.shader_data;
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

    builder.read(input.shadow_maps.dir_lights_shadow_map);
    builder.read(input.shadow_maps.point_lights_shadow_map);

    builder.read(input.skybox.skybox);
    builder.read(input.skybox.diffuse_irradiance);
    builder.read(input.skybox.specular_filtered);
    builder.read(input.skybox.brdf_lut);

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = fragment_shader_params_.mutable_typed_data<DeferredLightingPassParams>();
            params->gbuffer_textures[0] = {ctx.rg->texture(pass_data->gbuffer.base_color)};
            params->gbuffer_textures[1] = {ctx.rg->texture(pass_data->gbuffer.normal_roughness)};
            params->gbuffer_textures[2] = {ctx.rg->texture(pass_data->gbuffer.fresnel)};
            params->gbuffer_textures[3] = {ctx.rg->texture(pass_data->gbuffer.material_0)};
            params->depth_texture = {ctx.rg->texture(pass_data->depth)};
            params->gbuffer_sampler = {pass_data->gbuffer.get_sampler()};
            fragment_shader_params_.update_uniform_buffer();
            ctx.render_full_screen(camera, fragment_shader_, fragment_shader_params_);
        }
    );

    return OutputData{
        .output = pass_data->output,
    };
}

}

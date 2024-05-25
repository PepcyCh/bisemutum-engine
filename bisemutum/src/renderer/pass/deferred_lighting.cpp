#include "deferred_lighting.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(DeferredLightingPassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(Texture2D, gbuffer_textures, [GBufferTextures::count])
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_texture)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, gbuffer_sampler)

    BI_SHADER_PARAMETER_INCLUDE(LightsContextShaderData, lights_ctx)
    BI_SHADER_PARAMETER_INCLUDE(SkyboxContextShaderData, skybox_ctx)
    BI_SHADER_PARAMETER(DdgiVolumeLightingData, ddgi)
BI_SHADER_PARAMETERS_END()

struct PassData final {
    gfx::TextureHandle output;
    gfx::TextureHandle depth;
    GBufferTextures gbuffer;
    DdgiTextures ddgi;
};

}

DeferredLightingPass::DeferredLightingPass() {
    fragment_shader_params_.initialize<DeferredLightingPassParams>();

    fragment_shader_.source.path = "/bisemutum/shaders/renderer/deferred_lighting.hlsl";
    fragment_shader_.source.entry = "deferred_lighting_pass_fs";
    fragment_shader_.set_shader_params_struct<DeferredLightingPassParams>();
    fragment_shader_.depth_test = false;
    fragment_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    fragment_shader_.override_blend_mode = gfx::BlendMode::additive;
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

    pass_data->output = builder.use_color(0, input.color);

    builder.read(input.shadow_maps.dir_lights_shadow_map);
    builder.read(input.shadow_maps.point_lights_shadow_map);

    builder.read(input.skybox.skybox);
    builder.read(input.skybox.diffuse_irradiance);
    builder.read(input.skybox.specular_filtered);
    builder.read(input.skybox.brdf_lut);

    if (input.ddgi.irradiance != gfx::TextureHandle::invalid) {
        builder.read(input.ddgi.irradiance);
        builder.read(input.ddgi.visibility);
        pass_data->ddgi = input.ddgi;

        auto params = fragment_shader_params_.mutable_typed_data<DeferredLightingPassParams>();
        params->ddgi = input.ddgi_ctx.get_shader_data();
    } else {
        auto params = fragment_shader_params_.mutable_typed_data<DeferredLightingPassParams>();
        params->ddgi.num_volumes = 0;
    }

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = fragment_shader_params_.mutable_typed_data<DeferredLightingPassParams>();
            params->gbuffer_textures[0] = {ctx.rg->texture(pass_data->gbuffer.base_color)};
            params->gbuffer_textures[1] = {ctx.rg->texture(pass_data->gbuffer.normal_roughness)};
            params->gbuffer_textures[2] = {ctx.rg->texture(pass_data->gbuffer.fresnel)};
            params->gbuffer_textures[3] = {ctx.rg->texture(pass_data->gbuffer.material_0)};
            params->depth_texture = {ctx.rg->texture(pass_data->depth)};
            params->gbuffer_sampler = {pass_data->gbuffer.get_sampler()};
            if (pass_data->ddgi.irradiance != gfx::TextureHandle::invalid) {
                params->ddgi.irradiance_texture = {ctx.rg->texture(pass_data->ddgi.irradiance)};
                params->ddgi.visibility_texture = {ctx.rg->texture(pass_data->ddgi.visibility)};
            }
            fragment_shader_params_.update_uniform_buffer();
            ctx.render_full_screen(camera, fragment_shader_, fragment_shader_params_);
        }
    );

    return OutputData{
        .output = pass_data->output,
    };
}

}

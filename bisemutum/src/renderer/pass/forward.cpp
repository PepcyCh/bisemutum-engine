#include "forward.hpp"

#include "../context/lights.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(ForwardPassParams)
    BI_SHADER_PARAMETER(uint, num_dir_lights)
    BI_SHADER_PARAMETER(uint, num_point_lights)
    BI_SHADER_PARAMETER(uint, num_spot_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<DirLightData>, dir_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<LightData>, point_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<LightData>, spot_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float4x4>, dir_lights_shadow_transform)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2DArray, dir_lights_shadow_map)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, shadow_map_sampler)

    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox_diffuse_irradiance)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, skybox_sampler)
BI_SHADER_PARAMETERS_END(ForwardPassParams)

struct PassData final {
    gfx::TextureHandle output;
    gfx::TextureHandle depth;

    gfx::RenderedObjectListHandle list;
};

}

ForwardPass::ForwardPass() {
    fragmeng_shader_params.initialize<ForwardPassParams>();

    fragmeng_shader.source_path = "/bisemutum/shaders/renderer/forward_pass.hlsl";
    fragmeng_shader.source_entry = "forward_pass_fs";
    fragmeng_shader.set_shader_params_struct<ForwardPassParams>();
    fragmeng_shader.cull_mode = rhi::CullMode::back_face;
}

auto ForwardPass::update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void {
    auto params = fragmeng_shader_params.mutable_typed_data<ForwardPassParams>();
    params->num_dir_lights = lights_ctx.dir_lights.size();
    params->num_point_lights = lights_ctx.point_lights.size();
    params->num_spot_lights = lights_ctx.spot_lights.size();
    params->dir_lights = {&lights_ctx.dir_lights_buffer, 0};
    params->point_lights = {&lights_ctx.point_lights_buffer, 0};
    params->spot_lights = {&lights_ctx.spot_lights_buffer, 0};
    params->dir_lights_shadow_transform = {&lights_ctx.dir_lights_shadow_transform_buffer, 0};
    params->dir_lights_shadow_map = {&lights_ctx.dir_lights_shadow_map};
    params->shadow_map_sampler = {lights_ctx.shadow_map_sampler};

    params->skybox_diffuse_irradiance = {&skybox_ctx.diffuse_irradiance};
    params->skybox_sampler = {skybox_ctx.skybox_sampler};
}

auto ForwardPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData {
    auto& camera_target = camera.target_texture();

    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("Forward Pass");

    pass_data->output = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::rgba16_sfloat,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->depth = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::d24_unorm_s8_uint,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::depth_stencil_attachment, rhi::TextureUsage::sampled});
    });

    builder.use_color(
        0,
        gfx::GraphicsPassColorTargetBuilder{pass_data->output}.clear_color({0.2f, 0.3f, 0.5f, 1.0f})
    );
    builder.use_depth_stencil(gfx::GraphicsPassDepthStencilTargetBuilder{pass_data->depth}.clear_depth_stencil());

    builder.read(input.dir_lighst_shadow_map);

    builder.read(input.skybox.diffuse_irradiance);

    pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
        .camera = camera,
        .fragmeng_shader = fragmeng_shader,
        .type = gfx::RenderedObjectType::all,
    });

    fragmeng_shader_params.update_uniform_buffer();

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            ctx.render_list(pass_data->list, fragmeng_shader_params);
        }
    );

    return OutputData{
        .output = pass_data->output,
        .depth = pass_data->depth,
    };
}

}

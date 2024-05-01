#include "forward.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(ForwardPassParams)
    BI_SHADER_PARAMETER_INCLUDE(LightsContextShaderData, lights_ctx)
    BI_SHADER_PARAMETER_INCLUDE(SkyboxContextShaderData, skybox_ctx)
BI_SHADER_PARAMETERS_END()

struct PassData final {
    gfx::TextureHandle output;
    gfx::TextureHandle velocity;
    gfx::TextureHandle depth;

    gfx::RenderedObjectListHandle list;
};

}

ForwardPass::ForwardPass() {
    fragment_shader_params_.initialize<ForwardPassParams>();

    fragment_shader_.source.path = "/bisemutum/shaders/renderer/forward_pass.hlsl";
    fragment_shader_.source.entry = "forward_pass_fs";
    fragment_shader_.set_shader_params_struct<ForwardPassParams>();
    fragment_shader_.modify_compiler_environment_func = [](gfx::ShaderCompilationEnvironment& env) {
        env.set_define("FORWARD_OUTPUT_VELOCITY", 1);
    };
}

auto ForwardPass::update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void {
    auto params = fragment_shader_params_.mutable_typed_data<ForwardPassParams>();
    params->lights_ctx = lights_ctx.shader_data;
    params->skybox_ctx = skybox_ctx.shader_data;
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
                rhi::ResourceFormat::d32_sfloat_s8_uint,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::depth_stencil_attachment, rhi::TextureUsage::sampled});
    });

    pass_data->output = builder.use_color(
        0,
        gfx::GraphicsPassColorTargetBuilder{pass_data->output}.clear_color()
    );
    pass_data->depth = builder.use_depth_stencil(
        gfx::GraphicsPassDepthStencilTargetBuilder{pass_data->depth}.clear_depth_stencil()
    );

    pass_data->velocity = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::rg16_sfloat,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->velocity = builder.use_color(
        1,
        gfx::GraphicsPassColorTargetBuilder{pass_data->velocity}.clear_color()
    );

    builder.read(input.shadow_maps.dir_lights_shadow_map);
    builder.read(input.shadow_maps.point_lights_shadow_map);

    builder.read(input.skybox.diffuse_irradiance);
    builder.read(input.skybox.specular_filtered);
    builder.read(input.skybox.brdf_lut);

    pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
        .camera = camera,
        .fragment_shader = fragment_shader_,
        .type = gfx::RenderedObjectType::opaque,
        .candidate_drawables = input.drawables,
    });

    fragment_shader_params_.update_uniform_buffer();

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            ctx.render_list(pass_data->list, fragment_shader_params_);
        }
    );

    return OutputData{
        .output = pass_data->output,
        .velocity = pass_data->velocity,
        .depth = pass_data->depth,
    };
}


ForwardTransparentPass::ForwardTransparentPass() {
    fragment_shader_params_.initialize<ForwardPassParams>();

    fragment_shader_.source.path = "/bisemutum/shaders/renderer/forward_pass.hlsl";
    fragment_shader_.source.entry = "forward_pass_fs";
    fragment_shader_.set_shader_params_struct<ForwardPassParams>();
    fragment_shader_.depth_write = false;
}

auto ForwardTransparentPass::update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void {
    auto params = fragment_shader_params_.mutable_typed_data<ForwardPassParams>();
    params->lights_ctx = lights_ctx.shader_data;
    params->skybox_ctx = skybox_ctx.shader_data;
}

auto ForwardTransparentPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData {
    auto& camera_target = camera.target_texture();

    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("Forward Transparent Pass");

    pass_data->output = builder.use_color(0, input.color);
    pass_data->depth = builder.use_depth_stencil(input.depth);

    builder.read(input.shadow_maps.dir_lights_shadow_map);
    builder.read(input.shadow_maps.point_lights_shadow_map);

    builder.read(input.skybox.diffuse_irradiance);
    builder.read(input.skybox.specular_filtered);
    builder.read(input.skybox.brdf_lut);

    pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
        .camera = camera,
        .fragment_shader = fragment_shader_,
        .type = gfx::RenderedObjectType::transparent,
        .candidate_drawables = input.drawables,
        .sorting_mode = gfx::RendererObjectSortingMode::from_back_to_front,
    });

    fragment_shader_params_.update_uniform_buffer();

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            ctx.render_list(pass_data->list, fragment_shader_params_);
        }
    );

    return OutputData{
        .color = pass_data->output,
        .depth = pass_data->depth,
    };
}

}

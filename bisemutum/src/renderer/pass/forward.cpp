#include "forward.hpp"

#include "../context/lights.hpp"

namespace bi {

BI_SHADER_PARAMETERS_BEGIN(ForwardPassParams)
    BI_SHADER_PARAMETER(uint, num_dir_lights)
    BI_SHADER_PARAMETER(uint, num_point_lights)
    BI_SHADER_PARAMETER(uint, num_spot_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<LightData>, dir_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<LightData>, point_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<LightData>, spot_lights)
BI_SHADER_PARAMETERS_END(ForwardPassParams)

ForwardPass::ForwardPass() {
    fragmeng_shader_params.initialize<ForwardPassParams>();

    fragmeng_shader.source_path = "/engine/shaders/renderer/forward_pass.hlsl";
    fragmeng_shader.source_entry = "forward_pass_fs";
    fragmeng_shader.set_shader_params_struct<ForwardPassParams>();
    fragmeng_shader.cull_mode = rhi::CullMode::back_face;
}

auto ForwardPass::update_params(LightsContext& lights_ctx) -> void {
    auto params = fragmeng_shader_params.mutable_typed_data<ForwardPassParams>();
    params->num_dir_lights = lights_ctx.dir_lights.size();
    params->num_point_lights = lights_ctx.point_lights.size();
    params->num_spot_lights = lights_ctx.spot_lights.size();
    params->dir_lights = {&lights_ctx.dir_lights_buffer, 0};
    params->point_lights = {&lights_ctx.point_lights_buffer, 0};
    params->spot_lights = {&lights_ctx.spot_lights_buffer, 0};
}

auto ForwardPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg) -> Ref<PassData> {
    auto& camera_target = camera.target_texture();

    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("Forward Pass");

    // pass_data->output = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
    //     builder
    //         .dim_2d(
    //             rhi::ResourceFormat::rgba8_unorm,
    //             camera_target.desc().extent.width, camera_target.desc().extent.height
    //         )
    //         .usage(rhi::TextureUsage::color_attachment);
    // });
    pass_data->output = rg.import_back_buffer();
    pass_data->depth = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::d24_unorm_s8_uint,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage(rhi::TextureUsage::depth_stencil_attachment);
    });

    builder.use_color(
        0,
        gfx::GraphicsPassColorTargetBuilder{pass_data->output}.clear_color()
    );
    builder.use_depth_stencil(gfx::GraphicsPassDepthStencilTargetBuilder{pass_data->depth}.clear_depth_stencil());

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto list = ctx.get_rendered_object_list(gfx::RenderedObjectListDesc{
                .camera = camera,
                .fragmeng_shader = fragmeng_shader,
                .type = gfx::RenderedObjectType::all,
            });
            ctx.render_list(list, fragmeng_shader_params);
        }
    );

    return pass_data;
}

}

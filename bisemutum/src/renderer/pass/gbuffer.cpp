#include "gbuffer.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(GBufferPassParams)
BI_SHADER_PARAMETERS_END(GBufferPassParams)

struct PassData final {
    gfx::TextureHandle output;
    gfx::TextureHandle depth;
    GBufferTextures gbuffer;

    gfx::RenderedObjectListHandle list;
};

}

GBufferdPass::GBufferdPass() {
    fragment_shader_params_.initialize<GBufferPassParams>();

    fragment_shader_.source_path = "/bisemutum/shaders/renderer/gbuffer_pass.hlsl";
    fragment_shader_.source_entry = "gbuffer_pass_fs";
    fragment_shader_.set_shader_params_struct<GBufferPassParams>();
    fragment_shader_.cull_mode = rhi::CullMode::back_face;
}

auto GBufferdPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg) -> OutputData {
    auto& camera_target = camera.target_texture();

    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("GBuffer Pass");

    pass_data->output = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::rgba16_sfloat,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->gbuffer.add_textures(rg, camera_target.desc().extent.width, camera_target.desc().extent.height);
    pass_data->depth = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::d24_unorm_s8_uint,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::depth_stencil_attachment, rhi::TextureUsage::sampled});
    });

    pass_data->gbuffer.use_color(builder);
    builder.use_depth_stencil(gfx::GraphicsPassDepthStencilTargetBuilder{pass_data->depth}.clear_depth_stencil());

    pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
        .camera = camera,
        .fragment_shader = fragment_shader_,
        .type = gfx::RenderedObjectType::opaque,
    });

    fragment_shader_params_.update_uniform_buffer();

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            ctx.render_list(pass_data->list, fragment_shader_params_);
        }
    );

    return OutputData{
        .depth = pass_data->depth,
        .gbuffer = pass_data->gbuffer,
    };
}

}

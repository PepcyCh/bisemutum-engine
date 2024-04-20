#include "gbuffer.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(GBufferPassParams)
BI_SHADER_PARAMETERS_END()

struct PassData final {
    gfx::TextureHandle color;
    gfx::TextureHandle depth;
    gfx::TextureHandle velocity;
    GBufferTextures gbuffer;

    gfx::RenderedObjectListHandle list;
};

}

GBufferdPass::GBufferdPass() {
    fragment_shader_params_.initialize<GBufferPassParams>();

    fragment_shader_.source.path = "/bisemutum/shaders/renderer/gbuffer_pass.hlsl";
    fragment_shader_.source.entry = "gbuffer_pass_fs";
    fragment_shader_.set_shader_params_struct<GBufferPassParams>();
}

auto GBufferdPass::render(
    gfx::Camera const& camera, gfx::RenderGraph& rg, Span<Ref<gfx::Drawable>> drawables
) -> OutputData {
    auto& camera_target = camera.target_texture();

    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("GBuffer Pass");

    pass_data->gbuffer.add_textures(rg, camera_target.desc().extent.width, camera_target.desc().extent.height);
    pass_data->gbuffer.use_color(builder, 1);

    auto color = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::rgba16_sfloat,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->color = builder.use_color(0, gfx::GraphicsPassColorTargetBuilder{color}.clear_color());
    auto depth = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::d32_sfloat_s8_uint,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::depth_stencil_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->depth = builder.use_depth_stencil(
        gfx::GraphicsPassDepthStencilTargetBuilder{depth}.clear_depth_stencil()
    );

    auto velocity = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::rg16_sfloat,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->velocity = builder.use_color(
        GBufferTextures::count + 1,
        gfx::GraphicsPassColorTargetBuilder{velocity}.clear_color()
    );

    pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
        .camera = camera,
        .fragment_shader = fragment_shader_,
        .type = gfx::RenderedObjectType::opaque,
        .candidate_drawables = drawables,
    });

    fragment_shader_params_.update_uniform_buffer();

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            ctx.render_list(pass_data->list, fragment_shader_params_);
        }
    );

    return OutputData{
        .color = pass_data->color,
        .depth = pass_data->depth,
        .velocity = pass_data->velocity,
        .gbuffer = pass_data->gbuffer,
    };
}

}

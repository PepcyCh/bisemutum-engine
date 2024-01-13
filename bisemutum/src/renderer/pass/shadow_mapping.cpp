#include "shadow_mapping.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(ShadowMappingParams)
BI_SHADER_PARAMETERS_END(ShadowMappingParams)

struct PassData final {
    gfx::RenderedObjectListHandle list;
};

}

ShadowMappingPass::ShadowMappingPass() {
    fragment_shader_params.initialize<ShadowMappingParams>();

    fragment_shader.source_path = "/bisemutum/shaders/renderer/depth_only.hlsl";
    fragment_shader.source_entry = "depth_only_fs";
    fragment_shader.set_shader_params_struct<ShadowMappingParams>();
}

auto ShadowMappingPass::render(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input
) -> gfx::TextureHandle {
    auto dir_lights_shadow_map = rg.import_texture(input.lights_ctx.dir_lights_shadow_map);

    for (size_t index = 0; auto& dir_light : input.lights_ctx.dir_lights_with_shadow) {
        auto [builder, pass_data] = rg.add_graphics_pass<PassData>(
            fmt::format("Dir Light Shadow Map Pass #{}", index + 1)
        );
        builder.use_depth_stencil(
            gfx::GraphicsPassDepthStencilTargetBuilder{dir_lights_shadow_map}
                .clear_depth_stencil()
                .array_layer(index)
        );
        pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
            .camera = dir_light.camera,
            .fragment_shader = fragment_shader,
            .type = gfx::RenderedObjectType::opaque,
        });
        builder.set_execution_function<PassData>(
            [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                ctx.render_list(pass_data->list, fragment_shader_params);
            }
        );

        ++index;
    }

    return dir_lights_shadow_map;
}

}

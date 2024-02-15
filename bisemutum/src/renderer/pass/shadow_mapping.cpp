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
    fragment_shader_params_.initialize<ShadowMappingParams>();

    fragment_shader_.source_path = "/bisemutum/shaders/renderer/depth_only.hlsl";
    fragment_shader_.source_entry = "depth_only_fs";
    fragment_shader_.set_shader_params_struct<ShadowMappingParams>();
}

auto ShadowMappingPass::render(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input
) -> ShadowMapTextures {
    auto dir_lights_shadow_map = rg.import_texture(input.lights_ctx.dir_lights_shadow_map);
    for (size_t index = 0; auto& light : input.lights_ctx.dir_lights_with_shadow) {
        auto [builder, pass_data] = rg.add_graphics_pass<PassData>(
            fmt::format("Dir Light Shadow Map Pass #{}", index + 1)
        );
        dir_lights_shadow_map = builder.use_depth_stencil(
            gfx::GraphicsPassDepthStencilTargetBuilder{dir_lights_shadow_map}
                .clear_depth_stencil()
                .array_layer(index)
        );
        pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
            .camera = light.camera,
            .fragment_shader = fragment_shader_,
            .type = gfx::RenderedObjectType::opaque,
            .candidate_drawables = input.drawables,
        });
        builder.set_execution_function<PassData>(
            [this](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                ctx.render_list(pass_data->list, fragment_shader_params_);
            }
        );

        ++index;
    }

    auto point_lights_shadow_map = rg.import_texture(input.lights_ctx.point_lights_shadow_map);
    for (size_t index = 0; auto& light : input.lights_ctx.point_lights_with_shadow) {
        auto [builder, pass_data] = rg.add_graphics_pass<PassData>(
            fmt::format("Point Light Shadow Map Pass #{}", index + 1)
        );
        point_lights_shadow_map = builder.use_depth_stencil(
            gfx::GraphicsPassDepthStencilTargetBuilder{point_lights_shadow_map}
                .clear_depth_stencil()
                .array_layer(index)
        );
        pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
            .camera = light.camera,
            .fragment_shader = fragment_shader_,
            .type = gfx::RenderedObjectType::opaque,
            .candidate_drawables = input.drawables,
        });
        builder.set_execution_function<PassData>(
            [this](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                ctx.render_list(pass_data->list, fragment_shader_params_);
            }
        );

        ++index;
    }

    return ShadowMapTextures{
        .dir_lights_shadow_map = dir_lights_shadow_map,
        .point_lights_shadow_map = point_lights_shadow_map,
    };
}

}

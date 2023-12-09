#include "post_process.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi {

BI_SHADER_PARAMETERS_BEGIN(PostProcessPassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_color)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_depth)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, sampler_input)
BI_SHADER_PARAMETERS_END(PostProcessPassParams)

PostProcessPass::PostProcessPass() {
    fragmeng_shader_params.initialize<PostProcessPassParams>();

    fragmeng_shader.source_path = "/bisemutum/shaders/renderer/post_process.hlsl";
    fragmeng_shader.source_entry = "post_process_pass_fs";
    fragmeng_shader.set_shader_params_struct<PostProcessPassParams>();
    fragmeng_shader.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    fragmeng_shader.depth_test = false;

    sampler = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
    });
}

auto PostProcessPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> Ref<PassData> {
    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("Post Process Pass");

    pass_data->output = rg.import_back_buffer();
    builder.use_color(0, pass_data->output);

    pass_data->input_color = builder.read(input.color);
    pass_data->input_depth = builder.read(input.depth);

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = fragmeng_shader_params.mutable_typed_data<PostProcessPassParams>();
            params->input_color = {ctx.rg->texture(pass_data->input_color)};
            params->input_depth = {ctx.rg->texture(pass_data->input_depth)};
            params->sampler_input = {sampler.value()};
            fragmeng_shader_params.update_uniform_buffer();

            ctx.render_full_screen(camera, fragmeng_shader, fragmeng_shader_params);
        }
    );

    return pass_data;
}

}

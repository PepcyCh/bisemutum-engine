#include "depth_pyramid.hpp"

#include <bisemutum/prelude/math.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(DepthPyramidPass1Params)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER(uint, num_levels)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_depth_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE_ARRAY(RWTexture2D<float>, out_depth_tex, [7], rhi::ResourceFormat::r32_sfloat)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, depth_sampler)
BI_SHADER_PARAMETERS_END()

BI_SHADER_PARAMETERS_BEGIN(DepthPyramidPass2Params)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER(uint, num_levels)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float>, in_depth_tex, rhi::ResourceFormat::r32_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE_ARRAY(RWTexture2D<float>, out_depth_tex, [6], rhi::ResourceFormat::r32_sfloat)
BI_SHADER_PARAMETERS_END()

struct DepthPyramidPassData final {
    gfx::TextureHandle input;
    gfx::TextureHandle output;
};

}

DepthPyramidPass::DepthPyramidPass() {
    depth_pyramid_shader_1_params_.initialize<DepthPyramidPass1Params>();
    depth_pyramid_shader_1_.source.path = "/bisemutum/shaders/renderer/gen_depth_pyramid.hlsl";
    depth_pyramid_shader_1_.source.entry = "gen_depth_pyramid_cs";
    depth_pyramid_shader_1_.set_shader_params_struct<DepthPyramidPass1Params>();
    depth_pyramid_shader_1_.modify_compiler_environment_func = [](gfx::ShaderCompilationEnvironment& env) {
        env.set_define("FROM_DEPTH_TEXTURE");
    };

    depth_pyramid_shader_2_params_.initialize<DepthPyramidPass2Params>();
    depth_pyramid_shader_2_.source.path = "/bisemutum/shaders/renderer/gen_depth_pyramid.hlsl";
    depth_pyramid_shader_2_.source.entry = "gen_depth_pyramid_cs";
    depth_pyramid_shader_2_.set_shader_params_struct<DepthPyramidPass2Params>();

    sampler_ = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_edge,
    });
}

auto DepthPyramidPass::render(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input
) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto edge = 1u;
    auto num_levels = 1u;
    while (edge < width || edge < height) {
        edge <<= 1;
        ++num_levels;
    }

    auto depth_pyramid = rg.add_texture([edge](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::r32_sfloat, edge, edge)
            .mipmap()
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });

    {
        auto [builder, pass_data] = rg.add_compute_pass<DepthPyramidPassData>("Gen Depth Pyramid 1");

        pass_data->input = builder.read(input.depth);
        pass_data->output = builder.write(depth_pyramid);

        builder.set_execution_function<DepthPyramidPassData>(
            [this, width, height, edge, num_levels](CRef<DepthPyramidPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = depth_pyramid_shader_1_params_.mutable_typed_data<DepthPyramidPass1Params>();
                params->tex_size = {edge, edge};
                params->num_levels = num_levels;
                params->in_depth_tex = {ctx.rg->texture(pass_data->input)};
                params->depth_sampler = {sampler_};
                for (uint32_t i = 0; i < 7; i++) {
                    params->out_depth_tex[i] = {ctx.rg->texture(pass_data->output), std::min(i, num_levels - 1)};
                }
                depth_pyramid_shader_1_params_.update_uniform_buffer();
                ctx.dispatch(
                    depth_pyramid_shader_1_, depth_pyramid_shader_1_params_,
                    ceil_div(edge, 64u), ceil_div(edge, 64u)
                );
            }
        );
    }

    if (num_levels > 7) {
        auto [builder, pass_data] = rg.add_compute_pass<DepthPyramidPassData>("Gen Depth Pyramid 2");

        pass_data->output = builder.write(depth_pyramid);
        pass_data->input = pass_data->output;
        depth_pyramid = pass_data->output;

        width = std::max(1u, width >> 6);
        height = std::max(1u, height >> 6);
        edge = std::max(1u, edge >> 6);

        builder.set_execution_function<DepthPyramidPassData>(
            [this, width, height, edge, num_levels](CRef<DepthPyramidPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = depth_pyramid_shader_2_params_.mutable_typed_data<DepthPyramidPass2Params>();
                params->tex_size = {edge, edge};
                params->num_levels = num_levels - 6;
                params->in_depth_tex = {ctx.rg->texture(pass_data->input), 6};
                for (uint32_t i = 0; i < 6; i++) {
                    params->out_depth_tex[i] = {ctx.rg->texture(pass_data->output), std::min(7 + i, num_levels - 1)};
                }
                depth_pyramid_shader_2_params_.update_uniform_buffer();
                ctx.dispatch(
                    depth_pyramid_shader_2_, depth_pyramid_shader_2_params_,
                    ceil_div(edge, 64u), ceil_div(edge, 64u)
                );
            }
        );
    }

    return depth_pyramid;
}

}

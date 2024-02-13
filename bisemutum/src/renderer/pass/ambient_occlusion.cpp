#include "ambient_occlusion.hpp"

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(SSAOPassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END(SSAOPassParams)

struct SSAOPassData final {
    gfx::TextureHandle normal_roughness;
    gfx::TextureHandle depth;

    gfx::TextureHandle output;
};

BI_SHADER_PARAMETERS_BEGIN(MergePassParams)
    BI_SHADER_PARAMETER(float, ao_strength)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, color_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, ao_tex)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END(MergePassParams)

struct MergePassData final {
    gfx::TextureHandle color_tex;
    gfx::TextureHandle ao_tex;
    gfx::TextureHandle output;
};

}

AmbientOcclusionPass::AmbientOcclusionPass() {
    screen_space_shader_params_.initialize<SSAOPassParams>();
    screen_space_shader_.source_path = "/bisemutum/shaders/renderer/ambient_occlusion/ambient_occlusion_ss.hlsl";
    screen_space_shader_.source_entry = "ambient_occlusion_ss_fs";
    screen_space_shader_.set_shader_params_struct<SSAOPassParams>();
    screen_space_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    screen_space_shader_.depth_test = false;

    merge_ao_shader_params_.initialize<MergePassParams>();
    merge_ao_shader_.source_path = "/bisemutum/shaders/renderer/ambient_occlusion/merge.hlsl";
    merge_ao_shader_.source_entry = "merge_ao_fs";
    merge_ao_shader_.set_shader_params_struct<MergePassParams>();
    merge_ao_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    merge_ao_shader_.depth_test = false;

    sampler_ = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_edge,
    });
}

auto AmbientOcclusionPass::render(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::AmbientOcclusionSettings const& settings
) -> gfx::TextureHandle {
    gfx::TextureHandle ao_tex;
    switch (settings.mode) {
        case BasicRenderer::AmbientOcclusionSettings::Mode::screen_space:
            ao_tex = render_screen_space(camera, rg, input, settings);
            break;
        case BasicRenderer::AmbientOcclusionSettings::Mode::raytraced:
            ao_tex = render_raytraced(camera, rg, input, settings);
            break;
        default:
            unreachable();
    }

    // TODO - AO denoise

    auto result_tex = render_merge(camera, rg, settings, input.color, ao_tex);

    return result_tex;
}

auto AmbientOcclusionPass::render_screen_space(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::AmbientOcclusionSettings const& settings
) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto [builder, pass_data] = rg.add_graphics_pass<SSAOPassData>("SSAO Pass");

    pass_data->normal_roughness = builder.read(input.normal_roughness);
    pass_data->depth = builder.read(input.depth);
    auto ao_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rg16_sfloat, width, height)
            .usage({
                rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write,
            });
    });
    pass_data->output = builder.use_color(0, ao_tex);

    builder.set_execution_function<SSAOPassData>(
        [this, &camera](CRef<SSAOPassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = screen_space_shader_params_.mutable_typed_data<SSAOPassParams>();
            params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness)};
            params->depth_tex = {ctx.rg->texture(pass_data->depth)};
            params->input_sampler = {sampler_};
            screen_space_shader_params_.update_uniform_buffer();
            ctx.render_full_screen(camera, screen_space_shader_, screen_space_shader_params_);
        }
    );

    return ao_tex;
}

auto AmbientOcclusionPass::render_raytraced(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::AmbientOcclusionSettings const& settings
) -> gfx::TextureHandle {
    // TODO - RTAO
    return gfx::TextureHandle::invalid;
}

auto AmbientOcclusionPass::render_merge(
    gfx::Camera const& camera, gfx::RenderGraph& rg,
    BasicRenderer::AmbientOcclusionSettings const& settings,
    gfx::TextureHandle color_tex, gfx::TextureHandle ao_tex
) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto [builder, pass_data] = rg.add_graphics_pass<MergePassData>("Merge AO Pass");

    pass_data->color_tex = builder.read(color_tex);
    pass_data->ao_tex = builder.read(ao_tex);
    auto output_tex = rg.add_texture([&camera, width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(camera.target_texture().desc().format, width, height)
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->output = builder.use_color(0, output_tex);

    builder.set_execution_function<MergePassData>(
        [this, &camera, &settings](CRef<MergePassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = merge_ao_shader_params_.mutable_typed_data<MergePassParams>();
            params->color_tex = {ctx.rg->texture(pass_data->color_tex)};
            params->ao_tex = {ctx.rg->texture(pass_data->ao_tex)};
            params->input_sampler = {sampler_};
            params->ao_strength = settings.strength;
            merge_ao_shader_params_.update_uniform_buffer();
            ctx.render_full_screen(camera, merge_ao_shader_, merge_ao_shader_params_);
        }
    );

    return output_tex;
}

}

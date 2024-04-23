#include "ambient_occlusion.hpp"

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi {

namespace {

constexpr rhi::ResourceFormat ao_tex_format = rhi::ResourceFormat::rg16_sfloat;

BI_SHADER_PARAMETERS_BEGIN(SSAOPassParams)
    BI_SHADER_PARAMETER(float, ao_range)
    BI_SHADER_PARAMETER(float, ao_strength)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END()

struct SSAOPassData final {
    gfx::TextureHandle normal_roughness;
    gfx::TextureHandle depth;

    gfx::TextureHandle output;
};

BI_SHADER_PARAMETERS_BEGIN(RTAOPassParams)
    BI_SHADER_PARAMETER(float, ao_range)
    BI_SHADER_PARAMETER(float, ao_strength)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_SRV_ACCEL(RaytracingAccelerationStructure, scene_accel)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float2>, ao_tex, ao_tex_format)
BI_SHADER_PARAMETERS_END()

struct RTAOPassData final {
    gfx::AccelerationStructureHandle scene_accel;
    gfx::TextureHandle normal_roughness;
    gfx::TextureHandle depth;

    gfx::TextureHandle output;
};

BI_SHADER_PARAMETERS_BEGIN(TemporalAccumulatePassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float2>, ao_tex, ao_tex_format)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, history_ao_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, velocity_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<uint>, history_validation_tex, rhi::ResourceFormat::r8_uint)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END()

struct TemporalAccumulatePassData final {
    gfx::TextureHandle ao_value;
    gfx::TextureHandle history_ao_value;
    gfx::TextureHandle velocity;
    gfx::TextureHandle history_validation_tex;
};

BI_SHADER_PARAMETERS_BEGIN(SpatialFilterPassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_ao_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float2>, output_ao_tex, ao_tex_format)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END()

struct SpatialFilterPassData final {
    gfx::TextureHandle input;
    gfx::TextureHandle normal_roughness;
    gfx::TextureHandle depth;

    gfx::TextureHandle output;
};

BI_SHADER_PARAMETERS_BEGIN(MergePassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, color_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, ao_tex)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END()

struct MergePassData final {
    gfx::TextureHandle color_tex;
    gfx::TextureHandle ao_tex;
    gfx::TextureHandle output;
};

}

AmbientOcclusionPass::AmbientOcclusionPass() {
    screen_space_shader_params_.initialize<SSAOPassParams>();
    screen_space_shader_.source.path = "/bisemutum/shaders/renderer/ambient_occlusion/ambient_occlusion_ss.hlsl";
    screen_space_shader_.source.entry = "ambient_occlusion_ss_fs";
    screen_space_shader_.set_shader_params_struct<SSAOPassParams>();
    screen_space_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    screen_space_shader_.depth_test = false;

    if (g_engine->graphics_manager()->device()->properties().raytracing_pipeline) {
        raytracing_shader_params_.initialize<RTAOPassParams>();
        raytracing_shader_.source.path = "/bisemutum/shaders/renderer/ambient_occlusion/ambient_occlusion_rt.hlsl";
        raytracing_shader_.source.entry = "ambient_occlusion_rt_cs";
        raytracing_shader_.set_shader_params_struct<RTAOPassParams>();
    }

    temporal_accumulate_shader_params_.initialize<TemporalAccumulatePassParams>();
    temporal_accumulate_shader_.source.path = "/bisemutum/shaders/renderer/ambient_occlusion/temporal_accumulate.hlsl";
    temporal_accumulate_shader_.source.entry = "ao_temporal_accumulate_cs";
    temporal_accumulate_shader_.set_shader_params_struct<TemporalAccumulatePassParams>();

    spatial_filter_shader_params_.initialize<SpatialFilterPassParams>();
    spatial_filter_shader_.source.path = "/bisemutum/shaders/renderer/ambient_occlusion/spatial_filter.hlsl";
    spatial_filter_shader_.source.entry = "ao_spatial_filter_cs";
    spatial_filter_shader_.set_shader_params_struct<SpatialFilterPassParams>();

    merge_ao_shader_params_.initialize<MergePassParams>();
    merge_ao_shader_.source.path = "/bisemutum/shaders/renderer/ambient_occlusion/merge.hlsl";
    merge_ao_shader_.source.entry = "merge_ao_fs";
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

    auto mode = settings.mode;
    if (
        mode == BasicRenderer::AmbientOcclusionSettings::Mode::raytraced
        && input.scene_accel == gfx::AccelerationStructureHandle::invalid
    ) {
        log::warn("general", "Hardware raytracing is not supported but is used.");
        mode = BasicRenderer::AmbientOcclusionSettings::Mode::screen_space;
    }

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

    ao_tex = render_denoise(camera, rg, input, settings, ao_tex);

    auto result_tex = render_merge(camera, rg, input.color, ao_tex);

    return result_tex;
}

auto AmbientOcclusionPass::render_screen_space(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::AmbientOcclusionSettings const& settings
) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto ao_width = settings.half_resolution ? (width + 1) / 2 : width;
    auto ao_height = settings.half_resolution ? (height + 1) / 2 : height;

    auto [builder, pass_data] = rg.add_graphics_pass<SSAOPassData>("SSAO Pass");

    pass_data->normal_roughness = builder.read(input.normal_roughness);
    pass_data->depth = builder.read(input.depth);
    auto ao_tex = rg.add_texture([ao_width, ao_height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(ao_tex_format, ao_width, ao_height)
            .usage({
                rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write
            });
    });
    pass_data->output = builder.use_color(0, ao_tex);

    builder.set_execution_function<SSAOPassData>(
        [this, &camera, &settings](CRef<SSAOPassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = screen_space_shader_params_.mutable_typed_data<SSAOPassParams>();
            params->ao_range = std::max(settings.range, 0.05f);
            params->ao_strength = settings.strength;
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
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto ao_width = settings.half_resolution ? (width + 1) / 2 : width;
    auto ao_height = settings.half_resolution ? (height + 1) / 2 : height;

    auto [builder, pass_data] = rg.add_compute_pass<RTAOPassData>("RTAO Pass");

    pass_data->scene_accel = builder.read(input.scene_accel);
    pass_data->normal_roughness = builder.read(input.normal_roughness);
    pass_data->depth = builder.read(input.depth);
    auto ao_tex = rg.add_texture([ao_width, ao_height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(ao_tex_format, ao_width, ao_height)
            .usage({
                rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write
            });
    });
    pass_data->output = builder.write(ao_tex);

    builder.set_execution_function<RTAOPassData>(
        [this, &camera, &settings, ao_width, ao_height](CRef<RTAOPassData> pass_data, gfx::ComputePassContext const& ctx) {
            auto params = raytracing_shader_params_.mutable_typed_data<RTAOPassParams>();
            params->ao_range = std::max(settings.range, 0.05f);
            params->ao_strength = settings.strength;
            params->tex_size = {ao_width, ao_height};
            params->scene_accel = {ctx.rg->acceleration_structure(pass_data->scene_accel)};
            params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness)};
            params->depth_tex = {ctx.rg->texture(pass_data->depth)};
            params->input_sampler = {sampler_};
            params->ao_tex = {ctx.rg->texture(pass_data->output)};
            raytracing_shader_params_.update_uniform_buffer();
            ctx.dispatch(
                camera, raytracing_shader_, raytracing_shader_params_,
                ceil_div(ao_width, 16u), ceil_div(ao_height, 16u)
            );
        }
    );

    return ao_tex;
}

auto AmbientOcclusionPass::render_denoise(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::AmbientOcclusionSettings const& settings,
    gfx::TextureHandle ao_tex
) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto ao_width = rg.texture_desc(ao_tex)->extent.width;
    auto ao_height = rg.texture_desc(ao_tex)->extent.height;

    auto frame_count = g_engine->window()->frame_count();
    auto [frame_count_it, is_new_camera] = last_frame_counts_.try_emplace(&camera);
    auto has_history = !is_new_camera && frame_count_it->second + 1 == frame_count;
    frame_count_it->second = frame_count;
    auto history_ao_tex = camera.get_history_texture("ambient_occlusion");

    temporal_accumulate_shader_.modify_compiler_environment_func = [&settings](gfx::ShaderCompilationEnvironment& env) {
        env.set_define("AO_HALF_RESOLUTION", settings.half_resolution ? 1 : 0);
    };

    has_history &= history_ao_tex != gfx::TextureHandle::invalid
        && rg.texture_desc(history_ao_tex)->extent.width == ao_width
        && rg.texture_desc(history_ao_tex)->extent.height == ao_height;
    if (has_history) {
        auto [builder, pass_data] = rg.add_compute_pass<TemporalAccumulatePassData>("AO Temporal Accumulate Pass");

        pass_data->ao_value = builder.write(ao_tex);
        pass_data->history_ao_value = builder.read(history_ao_tex);
        ao_tex = pass_data->ao_value;
        pass_data->velocity = builder.read(input.velocity);
        pass_data->history_validation_tex = builder.read(input.history_validation);

        builder.set_execution_function<TemporalAccumulatePassData>(
            [this, ao_width, ao_height](CRef<TemporalAccumulatePassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = temporal_accumulate_shader_params_.mutable_typed_data<TemporalAccumulatePassParams>();
                params->tex_size = {ao_width, ao_height};
                params->ao_tex = {ctx.rg->texture(pass_data->ao_value)};
                params->history_ao_tex = {ctx.rg->texture(pass_data->history_ao_value)};
                params->velocity_tex = {ctx.rg->texture(pass_data->velocity)};
                params->history_validation_tex = {ctx.rg->texture(pass_data->history_validation_tex)};
                params->input_sampler = {sampler_};
                temporal_accumulate_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    temporal_accumulate_shader_, temporal_accumulate_shader_params_,
                    ceil_div(ao_width, 16u), ceil_div(ao_height, 16u)
                );
            }
        );
    }

    auto filtered_ao_tex = rg.add_texture([ao_width, ao_height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(ao_tex_format, ao_width, ao_height)
            .usage({rhi::TextureUsage::storage_read_write, rhi::TextureUsage::sampled});
    });

    {
        auto [builder, pass_data] = rg.add_compute_pass<SpatialFilterPassData>("AO Spatial Filter Pass");

        pass_data->input = builder.read(ao_tex);
        pass_data->depth = builder.read(input.depth);
        pass_data->normal_roughness = builder.read(input.normal_roughness);

        pass_data->output = builder.write(filtered_ao_tex);

        builder.set_execution_function<SpatialFilterPassData>(
            [this, ao_width, ao_height, &camera](
                CRef<SpatialFilterPassData> pass_data, gfx::ComputePassContext const& ctx
            ) {
                auto params = spatial_filter_shader_params_.mutable_typed_data<SpatialFilterPassParams>();
                params->tex_size = {ao_width, ao_height};
                params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness)};
                params->depth_tex = {ctx.rg->texture(pass_data->depth)};
                params->input_ao_tex = {ctx.rg->texture(pass_data->input)};
                params->output_ao_tex = {ctx.rg->texture(pass_data->output)};
                params->input_sampler = {sampler_};
                spatial_filter_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, spatial_filter_shader_, spatial_filter_shader_params_,
                    ceil_div(ao_width, 16u), ceil_div(ao_height, 16u)
                );
                camera.add_history_texture("ambient_occlusion", pass_data->output);
            }
        );
    }

    return filtered_ao_tex;
}

auto AmbientOcclusionPass::render_merge(
    gfx::Camera const& camera, gfx::RenderGraph& rg,
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
        [this, &camera](CRef<MergePassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = merge_ao_shader_params_.mutable_typed_data<MergePassParams>();
            params->color_tex = {ctx.rg->texture(pass_data->color_tex)};
            params->ao_tex = {ctx.rg->texture(pass_data->ao_tex)};
            params->input_sampler = {sampler_};
            merge_ao_shader_params_.update_uniform_buffer();
            ctx.render_full_screen(camera, merge_ao_shader_, merge_ao_shader_params_);
        }
    );

    return output_tex;
}

}

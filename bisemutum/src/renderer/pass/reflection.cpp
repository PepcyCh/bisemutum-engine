#include "reflection.hpp"

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

namespace bi {

namespace {

constexpr rhi::ResourceFormat reflection_tex_format = rhi::ResourceFormat::rgba16_sfloat;
constexpr rhi::ResourceFormat positions_tex_format = rhi::ResourceFormat::rgba32_sfloat;

BI_SHADER_PARAMETERS_BEGIN(DirectionSamplePassParams)
    BI_SHADER_PARAMETER(float, fade_roughness)
    BI_SHADER_PARAMETER(float, max_roughness)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(Texture2D, gbuffer_textures, [GBufferTextures::count])
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, gbuffer_sampler)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_weights, reflection_tex_format)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_directions, positions_tex_format)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_origins, positions_tex_format)
BI_SHADER_PARAMETERS_END()

struct DirectionSamplePassData final {
    gfx::TextureHandle depth;
    GBufferTextures gbuffer;
    gfx::TextureHandle ray_weights;
    gfx::TextureHandle ray_directions;
    gfx::TextureHandle ray_origins;
};

BI_SHADER_PARAMETERS_BEGIN(RtGBufferPassParams)
    BI_SHADER_PARAMETER(float, ray_length)
    BI_SHADER_PARAMETER_SRV_ACCEL(RaytracingAccelerationStructure, scene_accel)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, ray_origins)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, ray_directions)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, gbuffer_base_color, GBufferTextures::base_color_format)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, gbuffer_normal_roughness, GBufferTextures::normal_roughness_format)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, gbuffer_fresnel, GBufferTextures::fresnel_format)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, gbuffer_material_0, GBufferTextures::material_0_format)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, hit_positions, positions_tex_format)
BI_SHADER_PARAMETERS_END()

struct RtGBufferPassData final {
    gfx::AccelerationStructureHandle scene_accel;
    GBufferTextures gbuffer;
    gfx::TextureHandle ray_origins;
    gfx::TextureHandle ray_directions;
    gfx::TextureHandle hit_positions;
};

BI_SHADER_PARAMETERS_BEGIN(DeferredLightingSecondaryPassParams)
    BI_SHADER_PARAMETER(float, lighting_strength)
    BI_SHADER_PARAMETER(uint2, tex_size)

    BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(Texture2D, gbuffer_textures, [GBufferTextures::count])
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, gbuffer_sampler)

    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hit_positions)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, view_positions)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, weights)

    BI_SHADER_PARAMETER_INCLUDE(LightsContextShaderData, lights_ctx)
    BI_SHADER_PARAMETER_INCLUDE(SkyboxContextShaderData, skybox_ctx)

    BI_SHADER_PARAMETER(float3, skybox_color)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox)

    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, color_tex, reflection_tex_format)
BI_SHADER_PARAMETERS_END()

struct DeferredLightingSecondaryPassData final {
    gfx::TextureHandle color;
    GBufferTextures gbuffer;
    gfx::TextureHandle hit_positions;
    gfx::TextureHandle view_positions;
    gfx::TextureHandle weights;
};

BI_SHADER_PARAMETERS_BEGIN(MergePassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, color_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, reflection_tex)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END()

struct MergePassData final {
    gfx::TextureHandle color_tex;
    gfx::TextureHandle reflection_tex;
    gfx::TextureHandle output;
};

}

ReflectionPass::ReflectionPass() {
    // TODO - SSR

    if (g_engine->graphics_manager()->device()->properties().raytracing_pipeline) {
        rt_direction_sample_shader_params_.initialize<DirectionSamplePassParams>();
        rt_direction_sample_shader_.source.path = "/bisemutum/shaders/renderer/raytracing/direction_sample/specular_sample.hlsl";
        rt_direction_sample_shader_.source.entry = "specular_sample_cs";
        rt_direction_sample_shader_.set_shader_params_struct<DirectionSamplePassParams>();

        rt_gbuffer_shader_params_.initialize<RtGBufferPassParams>();
        rt_gbuffer_shader_.raygen_source.path = "/bisemutum/shaders/renderer/raytracing/rt_gbuffer.hlsl";
        rt_gbuffer_shader_.raygen_source.entry = "rt_gbuffer_rgen";
        rt_gbuffer_shader_.closest_hit_source.path = "/bisemutum/shaders/renderer/raytracing/hits/rt_gbuffer_hit.hlsl";
        rt_gbuffer_shader_.closest_hit_source.entry = "rt_gbuffer_rchit";
        rt_gbuffer_shader_.miss_source.path = "/bisemutum/shaders/renderer/raytracing/hits/rt_gbuffer_miss.hlsl";
        rt_gbuffer_shader_.miss_source.entry = "rt_gbuffer_rmiss";
        rt_gbuffer_shader_.set_shader_params_struct<RtGBufferPassParams>();

        rt_deferred_lighting_shader_params_.initialize<DeferredLightingSecondaryPassParams>();
        rt_deferred_lighting_shader_.source.path = "/bisemutum/shaders/renderer/raytracing/deferred_lighting_secondary.hlsl";
        rt_deferred_lighting_shader_.source.entry = "deferred_lighting_secondary_cs";
        rt_deferred_lighting_shader_.set_shader_params_struct<DeferredLightingSecondaryPassParams>();
    }

    merge_reflection_shader_params_.initialize<MergePassParams>();
    merge_reflection_shader_.source.path = "/bisemutum/shaders/renderer/reflection/merge.hlsl";
    merge_reflection_shader_.source.entry = "merge_reflection_fs";
    merge_reflection_shader_.set_shader_params_struct<MergePassParams>();
    merge_reflection_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    merge_reflection_shader_.depth_test = false;

    sampler_ = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_edge,
    });
}

auto ReflectionPass::update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void {
    if (rt_deferred_lighting_shader_params_) {
        auto params = rt_deferred_lighting_shader_params_.mutable_typed_data<DeferredLightingSecondaryPassParams>();

        params->lights_ctx = lights_ctx.shader_data;
        params->skybox_ctx = skybox_ctx.shader_data;

        auto& current_skybox = g_engine->system_manager()->get_system_for_current_scene<SkyboxSystem>()->current_skybox();
        params->skybox_color = current_skybox.color;
        params->skybox = {current_skybox.tex};
    }
}

auto ReflectionPass::render(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::ReflectionSettings const& settings
) -> gfx::TextureHandle {
    gfx::TextureHandle reflection_tex;

    auto mode = settings.mode;
    if (
        mode == BasicRenderer::ReflectionSettings::Mode::raytraced
        && input.scene_accel == gfx::AccelerationStructureHandle::invalid
    ) {
        log::warn("general", "Hardware raytracing is not supported but is used.");
        mode = BasicRenderer::ReflectionSettings::Mode::screen_space;
    }

    switch (settings.mode) {
        case BasicRenderer::ReflectionSettings::Mode::screen_space:
            reflection_tex = render_screen_space(camera, rg, input, settings);
            break;
        case BasicRenderer::ReflectionSettings::Mode::raytraced:
            reflection_tex = render_raytraced(camera, rg, input, settings);
            break;
        default:
            unreachable();
    }

    auto result_tex = render_merge(camera, rg, input.color, reflection_tex);

    return result_tex;
}

auto ReflectionPass::render_screen_space(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::ReflectionSettings const& settings
) -> gfx::TextureHandle {
    // TODO - SSR
    return gfx::TextureHandle::invalid;
}

auto ReflectionPass::render_raytraced(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::ReflectionSettings const& settings
) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    hit_gbuffer_texs_.add_textures(rg, width, height, {rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    hit_positions_tex_ = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(positions_tex_format, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });

    auto reflection_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(reflection_tex_format, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });
    auto ray_origins_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(positions_tex_format, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });
    auto ray_directions_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(positions_tex_format, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });
    auto ray_weights_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(reflection_tex_format, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });

    {
        auto [builder, pass_data] = rg.add_compute_pass<DirectionSamplePassData>("RTR Sample Direction");

        pass_data->depth = builder.read(input.depth);
        pass_data->gbuffer = input.gbuffer.read(builder);
        pass_data->ray_origins = builder.write(ray_origins_tex);
        pass_data->ray_directions = builder.write(ray_directions_tex);
        pass_data->ray_weights = builder.write(ray_weights_tex);

        builder.set_execution_function<DirectionSamplePassData>(
            [this, &camera, &settings, width, height](CRef<DirectionSamplePassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = rt_direction_sample_shader_params_.mutable_typed_data<DirectionSamplePassParams>();
                params->max_roughness = settings.max_roughness;
                params->fade_roughness = std::min(settings.fade_roughness, params->max_roughness - 0.0001f);
                params->tex_size = {width, height};
                params->depth_tex = {ctx.rg->texture(pass_data->depth)};
                params->gbuffer_textures[0] = {ctx.rg->texture(pass_data->gbuffer.base_color)};
                params->gbuffer_textures[1] = {ctx.rg->texture(pass_data->gbuffer.normal_roughness)};
                params->gbuffer_textures[2] = {ctx.rg->texture(pass_data->gbuffer.fresnel)};
                params->gbuffer_textures[3] = {ctx.rg->texture(pass_data->gbuffer.material_0)};
                params->gbuffer_sampler = {pass_data->gbuffer.get_sampler()};
                params->ray_origins = {ctx.rg->texture(pass_data->ray_origins)};
                params->ray_directions = {ctx.rg->texture(pass_data->ray_directions)};
                params->ray_weights = {ctx.rg->texture(pass_data->ray_weights)};
                rt_direction_sample_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, rt_direction_sample_shader_, rt_direction_sample_shader_params_,
                    ceil_div(width, 16u), ceil_div(height, 16u)
                );
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_raytracing_pass<RtGBufferPassData>("RTR Trace GBuffer");

        pass_data->scene_accel = builder.read(input.scene_accel);
        pass_data->hit_positions = builder.write(hit_positions_tex_);
        pass_data->gbuffer = hit_gbuffer_texs_.write(builder);
        pass_data->ray_origins = builder.read(ray_origins_tex);
        pass_data->ray_directions = builder.read(ray_directions_tex);

        builder.set_execution_function<RtGBufferPassData>(
            [this, &camera, &settings, width, height](CRef<RtGBufferPassData> pass_data, gfx::RaytracingPassContext const& ctx) {
                auto params = rt_gbuffer_shader_params_.mutable_typed_data<RtGBufferPassParams>();
                params->ray_length = settings.range;
                params->scene_accel = {ctx.rg->acceleration_structure(pass_data->scene_accel)};
                params->ray_origins = {ctx.rg->texture(pass_data->ray_origins)};
                params->ray_directions = {ctx.rg->texture(pass_data->ray_directions)};
                params->hit_positions = {ctx.rg->texture(pass_data->hit_positions)};
                params->gbuffer_base_color = {ctx.rg->texture(pass_data->gbuffer.base_color)};
                params->gbuffer_normal_roughness = {ctx.rg->texture(pass_data->gbuffer.normal_roughness)};
                params->gbuffer_fresnel = {ctx.rg->texture(pass_data->gbuffer.fresnel)};
                params->gbuffer_material_0 = {ctx.rg->texture(pass_data->gbuffer.material_0)};
                rt_gbuffer_shader_params_.update_uniform_buffer();
                ctx.dispatch_rays(
                    camera, rt_gbuffer_shader_, rt_gbuffer_shader_params_,
                    width, height
                );
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_compute_pass<DeferredLightingSecondaryPassData>("RTR Lighting");

        pass_data->color = builder.write(reflection_tex);
        pass_data->view_positions = builder.read(ray_origins_tex);
        pass_data->hit_positions = builder.read(hit_positions_tex_);
        pass_data->weights = builder.read(ray_weights_tex);
        pass_data->gbuffer = hit_gbuffer_texs_.read(builder);

        builder.set_execution_function<DeferredLightingSecondaryPassData>(
            [this, &camera, &settings, width, height](CRef<DeferredLightingSecondaryPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = rt_deferred_lighting_shader_params_.mutable_typed_data<DeferredLightingSecondaryPassParams>();
                params->lighting_strength = settings.strength;
                params->tex_size = {width, height};
                params->color_tex = {ctx.rg->texture(pass_data->color)};
                params->view_positions = {ctx.rg->texture(pass_data->view_positions)};
                params->hit_positions = {ctx.rg->texture(pass_data->hit_positions)};
                params->weights = {ctx.rg->texture(pass_data->weights)};
                params->gbuffer_textures[0] = {ctx.rg->texture(pass_data->gbuffer.base_color)};
                params->gbuffer_textures[1] = {ctx.rg->texture(pass_data->gbuffer.normal_roughness)};
                params->gbuffer_textures[2] = {ctx.rg->texture(pass_data->gbuffer.fresnel)};
                params->gbuffer_textures[3] = {ctx.rg->texture(pass_data->gbuffer.material_0)};
                params->gbuffer_sampler = {pass_data->gbuffer.get_sampler()};
                rt_deferred_lighting_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, rt_deferred_lighting_shader_, rt_deferred_lighting_shader_params_,
                    ceil_div(width, 16u), ceil_div(height, 16u)
                );
            }
        );
    }

    return reflection_tex;
}

auto ReflectionPass::render_denoise(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::ReflectionSettings const& settings,
    gfx::TextureHandle reflection_tex
) -> gfx::TextureHandle {
    // TODO - denoise reflection
    return reflection_tex;
}

auto ReflectionPass::render_merge(
    gfx::Camera const& camera, gfx::RenderGraph& rg,
    gfx::TextureHandle color_tex, gfx::TextureHandle reflection_tex
) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto [builder, pass_data] = rg.add_graphics_pass<MergePassData>("Merge Reflection Pass");

    pass_data->color_tex = builder.read(color_tex);
    pass_data->reflection_tex = builder.read(reflection_tex);
    auto output_tex = rg.add_texture([&camera, width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(camera.target_texture().desc().format, width, height)
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->output = builder.use_color(0, output_tex);

    builder.set_execution_function<MergePassData>(
        [this, &camera](CRef<MergePassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            auto params = merge_reflection_shader_params_.mutable_typed_data<MergePassParams>();
            params->color_tex = {ctx.rg->texture(pass_data->color_tex)};
            params->reflection_tex = {ctx.rg->texture(pass_data->reflection_tex)};
            params->input_sampler = {sampler_};
            merge_reflection_shader_params_.update_uniform_buffer();
            ctx.render_full_screen(camera, merge_reflection_shader_, merge_reflection_shader_params_);
        }
    );

    return output_tex;
}

}

#include "path_tracing.hpp"

#include <bisemutum/prelude/math.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>
#include <bisemutum/window/window.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(GenerateCameraRayPassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_weights, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_directions, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_origins, rhi::ResourceFormat::rgba32_sfloat)
BI_SHADER_PARAMETERS_END()

struct GenerateCameraRayPassData final {
    gfx::TextureHandle ray_origins;
    gfx::TextureHandle ray_directions;
    gfx::TextureHandle ray_weights;
};

BI_SHADER_PARAMETERS_BEGIN(SampleSecondaryRayPassParams)
    BI_SHADER_PARAMETER(uint, bounce_index)
    BI_SHADER_PARAMETER(float, _pad)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(Texture2D, gbuffer_textures, [GBufferTextures::count])
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hit_positions)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_weights, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_directions, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, ray_origins, rhi::ResourceFormat::rgba32_sfloat)
BI_SHADER_PARAMETERS_END()

struct SampleSecondaryRayPassData final {
    gfx::TextureHandle ray_origins;
    gfx::TextureHandle ray_directions;
    gfx::TextureHandle ray_weights;
    gfx::TextureHandle hit_positions;
    GBufferTextures gbuffer;
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
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, hit_positions, rhi::ResourceFormat::rgba32_sfloat)
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
    BI_SHADER_PARAMETER(float, _pad)
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

    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, color_tex, rhi::ResourceFormat::rgba16_sfloat)
BI_SHADER_PARAMETERS_END()

struct DeferredLightingSecondaryPassData final {
    gfx::TextureHandle color;
    GBufferTextures gbuffer;
    gfx::TextureHandle hit_positions;
    gfx::TextureHandle view_positions;
    gfx::TextureHandle weights;
};

BI_SHADER_PARAMETERS_BEGIN(BlitPassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, src_texture)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, sampler_src_texture)
BI_SHADER_PARAMETERS_END()

struct BlitPassData final {
    gfx::TextureHandle src_texture;
    gfx::TextureHandle dst_texture;
};

BI_SHADER_PARAMETERS_BEGIN(PtDepthPassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hit_positions)
BI_SHADER_PARAMETERS_END()

struct PtDepthPassData final {
    gfx::TextureHandle hit_positions;
};

BI_SHADER_PARAMETERS_BEGIN(AccumulatePassParams)
    BI_SHADER_PARAMETER(float, weight)
    BI_SHADER_PARAMETER(float, _pad)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, color_tex, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, history_color_tex)
BI_SHADER_PARAMETERS_END()

struct AccumulatePassData final {
    gfx::TextureHandle color_tex;
    gfx::TextureHandle history_color_tex;
};

} // namespace

PathTracingPass::PathTracingPass() {
    primary_ray_gen_shader_params_.initialize<GenerateCameraRayPassParams>();
    primary_ray_gen_shader_.source.path = "/bisemutum/shaders/renderer/raytracing/direction_sample/generate_camera_ray.hlsl";
    primary_ray_gen_shader_.source.entry = "generate_camera_ray_cs";
    primary_ray_gen_shader_.set_shader_params_struct<GenerateCameraRayPassParams>();

    direction_sample_shader_.source.path = "/bisemutum/shaders/renderer/raytracing/direction_sample/sample_secondary_ray.hlsl";
    direction_sample_shader_.source.entry = "sample_secondary_ray_cs";
    direction_sample_shader_.set_shader_params_struct<SampleSecondaryRayPassParams>();

    rt_gbuffer_shader_.raygen_source.path = "/bisemutum/shaders/renderer/raytracing/rt_gbuffer.hlsl";
    rt_gbuffer_shader_.raygen_source.entry = "rt_gbuffer_rgen";
    rt_gbuffer_shader_.closest_hit_source.path = "/bisemutum/shaders/renderer/raytracing/hits/rt_gbuffer_hit.hlsl";
    rt_gbuffer_shader_.closest_hit_source.entry = "rt_gbuffer_rchit";
    rt_gbuffer_shader_.any_hit_source.path = "/bisemutum/shaders/renderer/raytracing/hits/rt_gbuffer_hit.hlsl";
    rt_gbuffer_shader_.any_hit_source.entry = "rt_gbuffer_rahit";
    rt_gbuffer_shader_.miss_source.path = "/bisemutum/shaders/renderer/raytracing/hits/rt_gbuffer_miss.hlsl";
    rt_gbuffer_shader_.miss_source.entry = "rt_gbuffer_rmiss";
    rt_gbuffer_shader_.set_shader_params_struct<RtGBufferPassParams>();

    rt_deferred_lighting_shader_.source.path = "/bisemutum/shaders/renderer/raytracing/deferred_lighting_secondary.hlsl";
    rt_deferred_lighting_shader_.source.entry = "deferred_lighting_secondary_cs";
    rt_deferred_lighting_shader_.set_shader_params_struct<DeferredLightingSecondaryPassParams>();
    rt_deferred_lighting_shader_.modify_compiler_environment_func = [](gfx::ShaderCompilationEnvironment& env) {
        env.set_define("DEFERRED_LIGHTING_NO_IBL", 1);
    };

    blit_color_shader_.source.path = "/bisemutum/shaders/core/builtin/blit.hlsl";
    blit_color_shader_.source.entry = "blit_fs";
    blit_color_shader_.set_shader_params_struct<BlitPassParams>();
    blit_color_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    blit_color_shader_.depth_test = false;
    blit_color_shader_.override_blend_mode = gfx::BlendMode::additive;

    pt_depth_shader_params_.initialize<PtDepthPassParams>();
    pt_depth_shader_.source.path = "/bisemutum/shaders/renderer/raytracing/pt_depth.hlsl";
    pt_depth_shader_.source.entry = "pt_depth_fs";
    pt_depth_shader_.set_shader_params_struct<PtDepthPassParams>();
    pt_depth_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    pt_depth_shader_.depth_test = false;

    accumulate_shader_params_.initialize<AccumulatePassParams>();
    accumulate_shader_.source.path = "/bisemutum/shaders/renderer/raytracing/pt_accumulate.hlsl";
    accumulate_shader_.source.entry = "pt_accumulate_cs";
    accumulate_shader_.set_shader_params_struct<AccumulatePassParams>();

    sampler_ = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_edge,
    });
}

auto PathTracingPass::update_params(
    LightsContext& lights_ctx, SkyboxContext& skybox_ctx,
    BasicRenderer::PathTracingSettings const& settings
) -> void {
    auto max_bounces = std::clamp(settings.max_bounces, 2u, 16u);
    auto& current_skybox = g_engine->system_manager()->get_system_for_current_scene<SkyboxSystem>()->current_skybox();

    direction_sample_shader_params_.resize(max_bounces - 2);
    for (auto& params : direction_sample_shader_params_) {
        if (!params) {
            params.initialize<SampleSecondaryRayPassParams>();
        }
    }

    rt_gbuffer_shader_params_.resize(max_bounces - 1);
    for (auto& params : rt_gbuffer_shader_params_) {
        if (!params) {
            params.initialize<RtGBufferPassParams>();
        }
    }

    rt_deferred_lighting_shader_params_.resize(max_bounces - 1);
    for (auto& params : rt_deferred_lighting_shader_params_) {
        if (!params) {
            params.initialize<DeferredLightingSecondaryPassParams>();
        }
        auto params_data = params.mutable_typed_data<DeferredLightingSecondaryPassParams>();
        params_data->lights_ctx = lights_ctx.shader_data;
        params_data->skybox_ctx = skybox_ctx.shader_data;
        params_data->skybox_color = current_skybox.color;
        params_data->skybox = {current_skybox.tex};
    }

    blit_color_shader_params_.resize(max_bounces - 2);
    for (auto& params : blit_color_shader_params_) {
        if (!params) {
            params.initialize<BlitPassParams>();
        }
    }
}

auto PathTracingPass::render(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
    BasicRenderer::PathTracingSettings const& settings
) -> OutputData {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto frame_count = g_engine->window()->frame_count();
    auto [hist_camera_it, is_new_camera] = camera_history_infos_.try_emplace(&camera);
    auto has_history = !is_new_camera
        && hist_camera_it->second.last_frame + 1 == frame_count
        && hist_camera_it->second.width == width
        && hist_camera_it->second.height == height
        && hist_camera_it->second.proj_view == camera.matrix_proj_view();
    hist_camera_it->second.last_frame = frame_count;
    hist_camera_it->second.width = width;
    hist_camera_it->second.height = height;
    hist_camera_it->second.proj_view = camera.matrix_proj_view();
    if (has_history) {
        ++hist_camera_it->second.frame_count;
    } else {
        hist_camera_it->second.frame_count = 1;
    }

    auto color_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write, rhi::TextureUsage::color_attachment});
    });
    auto temp_color_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });
    auto depth_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::d32_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::depth_stencil_attachment});
    });

    GBufferTextures primary_gbuffer_texs{};
    primary_gbuffer_texs.add_textures(rg, width, height, {rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    GBufferTextures hit_gbuffer_texs{};
    hit_gbuffer_texs.add_textures(rg, width, height, {rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});

    auto hit_positions_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba32_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });
    auto ray_origins_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba32_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });
    auto ray_directions_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });
    auto ray_weights_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });

    auto max_bounces = std::clamp(settings.max_bounces, 2u, 16u);
    for (uint32_t i = 1; i < max_bounces; i++) {
        if (i == 1) {
            auto [builder, pass_data] = rg.add_compute_pass<GenerateCameraRayPassData>("PT Generate Camera Ray");
            pass_data->ray_origins = builder.write(ray_origins_tex);
            pass_data->ray_directions = builder.write(ray_directions_tex);
            pass_data->ray_weights = builder.write(ray_weights_tex);
            builder.set_execution_function<GenerateCameraRayPassData>(
                [this, &camera, width, height](CRef<GenerateCameraRayPassData> pass_data, gfx::ComputePassContext const& ctx) {
                    auto params = primary_ray_gen_shader_params_.mutable_typed_data<GenerateCameraRayPassParams>();
                    params->tex_size = {width, height};
                    params->ray_origins = {ctx.rg->texture(pass_data->ray_origins)};
                    params->ray_directions = {ctx.rg->texture(pass_data->ray_directions)};
                    params->ray_weights = {ctx.rg->texture(pass_data->ray_weights)};
                    primary_ray_gen_shader_params_.update_uniform_buffer();
                    ctx.dispatch(
                        camera, primary_ray_gen_shader_, primary_ray_gen_shader_params_,
                        ceil_div(width, 16u), ceil_div(height, 16u)
                    );
                }
            );
        } else {
            auto [builder, pass_data] = rg.add_compute_pass<SampleSecondaryRayPassData>(
                fmt::format("PT Sample Ray #{}", i)
            );
            pass_data->hit_positions = builder.read(hit_positions_tex);
            pass_data->ray_origins = builder.write(ray_origins_tex);
            ray_origins_tex = pass_data->ray_origins;
            pass_data->ray_directions = builder.write(ray_directions_tex);
            ray_directions_tex = pass_data->ray_directions;
            pass_data->ray_weights = builder.write(ray_weights_tex);
            ray_weights_tex = pass_data->ray_weights;
            pass_data->gbuffer = (i == 2 ? primary_gbuffer_texs : hit_gbuffer_texs).read(builder);
            builder.set_execution_function<SampleSecondaryRayPassData>(
                [this, &camera, width, height, i](CRef<SampleSecondaryRayPassData> pass_data, gfx::ComputePassContext const& ctx) {
                    auto params = direction_sample_shader_params_[i - 2].mutable_typed_data<SampleSecondaryRayPassParams>();
                    params->tex_size = {width, height};
                    params->bounce_index = i - 1;
                    params->hit_positions = {ctx.rg->texture(pass_data->hit_positions)};
                    params->ray_origins = {ctx.rg->texture(pass_data->ray_origins)};
                    params->ray_directions = {ctx.rg->texture(pass_data->ray_directions)};
                    params->ray_weights = {ctx.rg->texture(pass_data->ray_weights)};
                    params->gbuffer_textures[0] = {ctx.rg->texture(pass_data->gbuffer.base_color)};
                    params->gbuffer_textures[1] = {ctx.rg->texture(pass_data->gbuffer.normal_roughness)};
                    params->gbuffer_textures[2] = {ctx.rg->texture(pass_data->gbuffer.fresnel)};
                    params->gbuffer_textures[3] = {ctx.rg->texture(pass_data->gbuffer.material_0)};
                    direction_sample_shader_params_[i - 2].update_uniform_buffer();
                    ctx.dispatch(
                        camera, direction_sample_shader_, direction_sample_shader_params_[i - 2],
                        ceil_div(width, 16u), ceil_div(height, 16u)
                    );
                }
            );
        }

        {
            auto [builder, pass_data] = rg.add_raytracing_pass<RtGBufferPassData>(
                fmt::format("PT Trace GBuffer #{}", i)
            );

            pass_data->scene_accel = builder.read(input.scene_accel);
            pass_data->hit_positions = builder.write(hit_positions_tex);
            hit_positions_tex = pass_data->hit_positions;
            if (i == 1) {
                pass_data->gbuffer = primary_gbuffer_texs.write(builder);
            } else {
                pass_data->gbuffer = hit_gbuffer_texs.write(builder);
                hit_gbuffer_texs = pass_data->gbuffer;
            }
            pass_data->ray_origins = builder.read(ray_origins_tex);
            pass_data->ray_directions = builder.read(ray_directions_tex);

            builder.set_execution_function<RtGBufferPassData>(
                [this, &camera, &settings, width, height, i](CRef<RtGBufferPassData> pass_data, gfx::RaytracingPassContext const& ctx) {
                    auto params = rt_gbuffer_shader_params_[i - 1].mutable_typed_data<RtGBufferPassParams>();
                    params->ray_length = settings.ray_length;
                    params->scene_accel = {ctx.rg->acceleration_structure(pass_data->scene_accel)};
                    params->ray_origins = {ctx.rg->texture(pass_data->ray_origins)};
                    params->ray_directions = {ctx.rg->texture(pass_data->ray_directions)};
                    params->hit_positions = {ctx.rg->texture(pass_data->hit_positions)};
                    params->gbuffer_base_color = {ctx.rg->texture(pass_data->gbuffer.base_color)};
                    params->gbuffer_normal_roughness = {ctx.rg->texture(pass_data->gbuffer.normal_roughness)};
                    params->gbuffer_fresnel = {ctx.rg->texture(pass_data->gbuffer.fresnel)};
                    params->gbuffer_material_0 = {ctx.rg->texture(pass_data->gbuffer.material_0)};
                    rt_gbuffer_shader_params_[i - 1].update_uniform_buffer();
                    ctx.dispatch_rays(
                        camera, rt_gbuffer_shader_, rt_gbuffer_shader_params_[i - 1],
                        width, height
                    );
                }
            );
        }

        {
            auto [builder, pass_data] = rg.add_compute_pass<DeferredLightingSecondaryPassData>(
                fmt::format("PT Lighting #{}", i)
            );

            if (i == 1) {
                pass_data->color = builder.write(color_tex);
            } else {
                pass_data->color = builder.write(temp_color_tex);
                temp_color_tex = pass_data->color;
            }
            pass_data->view_positions = builder.read(ray_origins_tex);
            pass_data->hit_positions = builder.read(hit_positions_tex);
            pass_data->weights = builder.read(ray_weights_tex);
            pass_data->gbuffer = (i == 1 ? primary_gbuffer_texs : hit_gbuffer_texs).read(builder);
            builder.read(input.shadow_maps.dir_lights_shadow_map);
            builder.read(input.shadow_maps.point_lights_shadow_map);

            builder.set_execution_function<DeferredLightingSecondaryPassData>(
                [this, &camera, width, height, i, max_bounces](CRef<DeferredLightingSecondaryPassData> pass_data, gfx::ComputePassContext const& ctx) {
                    auto params = rt_deferred_lighting_shader_params_[i - 1].mutable_typed_data<DeferredLightingSecondaryPassParams>();
                    params->lighting_strength = 1.0f;
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
                    rt_deferred_lighting_shader_params_[i - 1].update_uniform_buffer();
                    ctx.dispatch(
                        camera, rt_deferred_lighting_shader_, rt_deferred_lighting_shader_params_[i - 1],
                        ceil_div(width, 16u), ceil_div(height, 16u)
                    );

                    if (max_bounces == 2) {
                        camera.add_history_texture("path_tracing_color", pass_data->color);
                    }
                }
            );
        }

        if (i == 1) {
            auto [builder, pass_data] = rg.add_graphics_pass<PtDepthPassData>("PT Depth");
            pass_data->hit_positions = builder.read(hit_positions_tex);
            depth_tex = builder.use_depth_stencil(depth_tex);
            builder.set_execution_function<PtDepthPassData>(
                [this, &camera, i](CRef<PtDepthPassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                    auto params = pt_depth_shader_params_.mutable_typed_data<PtDepthPassParams>();
                    params->hit_positions = {ctx.rg->texture(pass_data->hit_positions)};
                    pt_depth_shader_params_.update_uniform_buffer();
                    ctx.render_full_screen(camera, pt_depth_shader_, pt_depth_shader_params_);
                }
            );
        } else {
            auto [builder, pass_data] = rg.add_graphics_pass<BlitPassData>(
                fmt::format("PT Blit Color #{}", i)
            );
            pass_data->src_texture = builder.read(temp_color_tex);
            pass_data->dst_texture = builder.use_color(0, color_tex);
            color_tex = pass_data->dst_texture;
            builder.set_execution_function<BlitPassData>(
                [this, &camera, i, max_bounces](CRef<BlitPassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                    auto params = blit_color_shader_params_[i - 2].mutable_typed_data<BlitPassParams>();
                    params->src_texture = {ctx.rg->texture(pass_data->src_texture)};
                    params->sampler_src_texture = {sampler_};
                    blit_color_shader_params_[i - 2].update_uniform_buffer();
                    ctx.render_full_screen(camera, blit_color_shader_, blit_color_shader_params_[i - 2]);

                    if (i + 1 == max_bounces) {
                        camera.add_history_texture("path_tracing_color", pass_data->dst_texture);
                    }
                }
            );
        }
    }

    auto history_color_tex = camera.get_history_texture("path_tracing_color");
    if (has_history && settings.accumulate && history_color_tex != gfx::TextureHandle::invalid) {
        auto [builder, pass_data] = rg.add_compute_pass<AccumulatePassData>("PT Accumulate");
        pass_data->color_tex = builder.write(color_tex);
        color_tex = pass_data->color_tex;
        pass_data->history_color_tex = builder.read(history_color_tex);
        builder.set_execution_function<AccumulatePassData>(
            [this, &camera, width, height, hist_camera_it](CRef<AccumulatePassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = accumulate_shader_params_.mutable_typed_data<AccumulatePassParams>();
                params->tex_size = {width, height};
                params->weight = 1.0f / hist_camera_it->second.frame_count;
                params->color_tex = {ctx.rg->texture(pass_data->color_tex)};
                params->history_color_tex = {ctx.rg->texture(pass_data->history_color_tex)};
                accumulate_shader_params_.update_uniform_buffer();
                ctx.dispatch(accumulate_shader_, accumulate_shader_params_, ceil_div(width, 16u), ceil_div(height, 16u));
            }
        );
    }

    return OutputData{
        .color = color_tex,
        .depth = depth_tex,
        .velocity = gfx::TextureHandle::invalid,
        .gbuffer = primary_gbuffer_texs,
    };
}

}

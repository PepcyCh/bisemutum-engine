#include "ddgi_update.hpp"

#include <bisemutum/prelude/math.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>
#include <bisemutum/window/window.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(TraceGBufferParams)
    BI_SHADER_PARAMETER(DdgiVolumeData, volume)
    BI_SHADER_PARAMETER(float, ray_length)
    BI_SHADER_PARAMETER_SRV_ACCEL(RaytracingAccelerationStructure, scene_accel)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float2>, sample_randoms)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, probe_gbuffer_base_color, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, probe_gbuffer_normal_roughness, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, probe_gbuffer_position, rhi::ResourceFormat::rgba32_sfloat)
BI_SHADER_PARAMETERS_END()

struct TraceGBufferPassData final {
    gfx::AccelerationStructureHandle scene_accel;
    gfx::BufferHandle sample_randoms;
    gfx::TextureHandle probe_gbuffer_base_color;
    gfx::TextureHandle probe_gbuffer_normal_roughness;
    gfx::TextureHandle probe_gbuffer_position;
};

BI_SHADER_PARAMETERS_BEGIN(DeferredLightingParams)
    BI_SHADER_PARAMETER(DdgiVolumeData, volume)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, probe_gbuffer_base_color)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, probe_gbuffer_normal_roughness)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, probe_gbuffer_position)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, trace_radiance, rhi::ResourceFormat::rgba16_sfloat)

    BI_SHADER_PARAMETER_INCLUDE(LightsContextShaderData, lights_ctx)
    BI_SHADER_PARAMETER_INCLUDE(SkyboxContextShaderData, skybox_ctx)
    BI_SHADER_PARAMETER(DdgiVolumeLightingData, ddgi)

    BI_SHADER_PARAMETER(float3, skybox_color)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox)
BI_SHADER_PARAMETERS_END()

struct DeferredLightingPassData final {
    gfx::TextureHandle probe_gbuffer_base_color;
    gfx::TextureHandle probe_gbuffer_normal_roughness;
    gfx::TextureHandle probe_gbuffer_position;
    gfx::TextureHandle trace_radiance;
};

BI_SHADER_PARAMETERS_BEGIN(ProbeBlendIrradianceParams)
    BI_SHADER_PARAMETER(uint, history_valid)
    BI_SHADER_PARAMETER(DdgiVolumeData, volume)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, trace_radiance)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, trace_gbuffer_position)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, history_probe_irradiance)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, probe_irradiance, rhi::ResourceFormat::rgba16_sfloat)
BI_SHADER_PARAMETERS_END()

struct ProbeBlendIrradiancePassData final {
    gfx::TextureHandle trace_gbuffer_position;
    gfx::TextureHandle trace_radiance;
    gfx::TextureHandle probe_irradiance;
    gfx::TextureHandle history_probe_irradiance;
};

BI_SHADER_PARAMETERS_BEGIN(ProbeBlendVisibilityParams)
    BI_SHADER_PARAMETER(uint, history_valid)
    BI_SHADER_PARAMETER(DdgiVolumeData, volume)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, trace_gbuffer_position)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, history_probe_visibility)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float2>, probe_visibility, rhi::ResourceFormat::rg16_sfloat)
BI_SHADER_PARAMETERS_END()

struct ProbeBlendVisibilityPassData final {
    gfx::TextureHandle trace_gbuffer_position;
    gfx::TextureHandle probe_visibility;
    gfx::TextureHandle history_probe_visibility;
};

} // namespace

DdgiUpdatePass::DdgiUpdatePass() {
    trace_gbuffer_shader_.raygen_source.path = "/bisemutum/shaders/renderer/ddgi/trace_gbuffer.hlsl";
    trace_gbuffer_shader_.raygen_source.entry = "ddgi_trace_gbuffer_rgen";
    trace_gbuffer_shader_.closest_hit_source.path = "/bisemutum/shaders/renderer/raytracing/hits/rt_gbuffer_hit.hlsl";
    trace_gbuffer_shader_.closest_hit_source.entry = "rt_gbuffer_rchit";
    trace_gbuffer_shader_.any_hit_source.path = "/bisemutum/shaders/renderer/raytracing/hits/rt_gbuffer_hit.hlsl";
    trace_gbuffer_shader_.any_hit_source.entry = "rt_gbuffer_rahit";
    trace_gbuffer_shader_.miss_source.path = "/bisemutum/shaders/renderer/raytracing/hits/rt_gbuffer_miss.hlsl";
    trace_gbuffer_shader_.miss_source.entry = "rt_gbuffer_rmiss";
    trace_gbuffer_shader_.set_shader_params_struct<TraceGBufferParams>();

    deferred_lighting_shader_.source.path = "/bisemutum/shaders/renderer/ddgi/deferred_lighting.hlsl";
    deferred_lighting_shader_.source.entry = "ddgi_deferred_lighting_cs";
    deferred_lighting_shader_.set_shader_params_struct<DeferredLightingParams>();

    probe_blend_irradiance_shader_.source.path = "/bisemutum/shaders/renderer/ddgi/probe_blend_irradiance.hlsl";
    probe_blend_irradiance_shader_.source.entry = "ddgi_probe_blend_irradiance_cs";
    probe_blend_irradiance_shader_.set_shader_params_struct<ProbeBlendIrradianceParams>();

    probe_blend_visibility_shader_.source.path = "/bisemutum/shaders/renderer/ddgi/probe_blend_visibility.hlsl";
    probe_blend_visibility_shader_.source.entry = "ddgi_probe_blend_visibility_cs";
    probe_blend_visibility_shader_.set_shader_params_struct<ProbeBlendVisibilityParams>();
}

auto DdgiUpdatePass::update_params(
    DdgiContext& ddgi_ctx, LightsContext& lights_ctx, SkyboxContext& skybox_ctx
) -> void {
    auto num_volumes = ddgi_ctx.num_ddgi_volumes();
    auto& current_skybox = g_engine->system_manager()->get_system_for_current_scene<SkyboxSystem>()->current_skybox();

    trace_gbuffer_shader_params_.resize(num_volumes);
    for (auto& params : trace_gbuffer_shader_params_) {
        if (!params) {
            params.initialize<TraceGBufferParams>();
        }
    }

    deferred_lighting_shader_params_.resize(num_volumes);
    for (auto& params : deferred_lighting_shader_params_) {
        if (!params) {
            params.initialize<DeferredLightingParams>();
        }
        auto params_data = params.mutable_typed_data<DeferredLightingParams>();
        params_data->lights_ctx = lights_ctx.shader_data;
        params_data->skybox_ctx = skybox_ctx.shader_data;
        params_data->skybox_color = current_skybox.color;
        params_data->skybox = {current_skybox.tex};
        params_data->ddgi = ddgi_ctx.get_shader_data_prev();
    }

    probe_blend_irradiance_shader_params_.resize(num_volumes);
    for (auto& params : probe_blend_irradiance_shader_params_) {
        if (!params) {
            params.initialize<ProbeBlendIrradianceParams>();
        }
    }

    probe_blend_visibility_shader_params_.resize(num_volumes);
    for (auto& params : probe_blend_visibility_shader_params_) {
        if (!params) {
            params.initialize<ProbeBlendVisibilityParams>();
        }
    }
}

auto DdgiUpdatePass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> DdgiTextures {
    auto this_frame = g_engine->window()->frame_count();
    if (last_frame_ == this_frame) {
        auto irradiance = rg.import_texture(input.ddgi_ctx.get_irradiance_texture());
        auto visibility = rg.import_texture(input.ddgi_ctx.get_visibility_texture());
        return {
            .irradiance = irradiance,
            .visibility = visibility,
        };
    }

    auto irradiance = rg.add_texture([&input](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                DdgiContext::irradiance_texture_format,
                DdgiContext::irradiance_texture_width,
                DdgiContext::irradiance_texture_height,
                input.ddgi_ctx.num_ddgi_volumes()
            )
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });
    auto visibility = rg.add_texture([&input](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                DdgiContext::visibility_texture_format,
                DdgiContext::visibility_texture_width,
                DdgiContext::visibility_texture_height,
                input.ddgi_ctx.num_ddgi_volumes()
            )
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });

    auto history_irradiance = gfx::TextureHandle::invalid;
    auto history_visibility = gfx::TextureHandle::invalid;
    if (input.ddgi_ctx.is_texture_prev_valid() && last_frame_ + 1 == this_frame) {
        history_irradiance = rg.import_texture(input.ddgi_ctx.get_irradiance_texture_prev());
        history_visibility = rg.import_texture(input.ddgi_ctx.get_visibility_texture_prev());
    }

    auto sample_randoms = rg.import_buffer(input.ddgi_ctx.sample_randoms_buffer());
    size_t curr_volume_index = 0;

    input.ddgi_ctx.for_each_ddgi_volume([&](DdgiVolumeInfo const& volume_data) {
        ++curr_volume_index;

        auto trace_gbuffer_base_color = rg.add_texture([](gfx::TextureBuilder& builder) {
            builder
                .dim_2d(
                    rhi::ResourceFormat::rgba16_sfloat,
                    DdgiContext::num_rays_per_probe, DdgiContext::probes_total_count
                )
                .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
        });
        auto trace_gbuffer_normal_roughness = rg.add_texture([](gfx::TextureBuilder& builder) {
            builder
                .dim_2d(
                    rhi::ResourceFormat::rgba16_sfloat,
                    DdgiContext::num_rays_per_probe, DdgiContext::probes_total_count
                )
                .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
        });
        auto trace_gbuffer_position = rg.add_texture([](gfx::TextureBuilder& builder) {
            builder
                .dim_2d(
                    rhi::ResourceFormat::rgba32_sfloat,
                    DdgiContext::num_rays_per_probe, DdgiContext::probes_total_count
                )
                .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
        });
        auto trace_radiance = rg.add_texture([](gfx::TextureBuilder& builder) {
            builder
                .dim_2d(
                    rhi::ResourceFormat::rgba16_sfloat,
                    DdgiContext::num_rays_per_probe, DdgiContext::probes_total_count
                )
                .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
        });

        {
            auto [builder, pass_data] = rg.add_raytracing_pass<TraceGBufferPassData>(
                fmt::format("DDGI Trace GBuffer #{}", volume_data.index)
            );

            pass_data->scene_accel = builder.read(input.scene_accel);
            pass_data->sample_randoms = builder.read(sample_randoms);
            pass_data->probe_gbuffer_base_color = builder.write(trace_gbuffer_base_color);
            pass_data->probe_gbuffer_normal_roughness = builder.write(trace_gbuffer_normal_roughness);
            pass_data->probe_gbuffer_position = builder.write(trace_gbuffer_position);

            builder.set_execution_function<TraceGBufferPassData>(
                [this, &camera, &volume_data](CRef<TraceGBufferPassData> pass_data, gfx::RaytracingPassContext const& ctx) {
                    auto params = trace_gbuffer_shader_params_[volume_data.index].mutable_typed_data<TraceGBufferParams>();
                    params->ray_length = 1024.0f;
                    params->volume.base_position = volume_data.base_position;
                    params->volume.frame_x = volume_data.frame_x;
                    params->volume.frame_y = volume_data.frame_y;
                    params->volume.frame_z = volume_data.frame_z;
                    params->volume.extent_x = volume_data.voluem_extent.x;
                    params->volume.extent_y = volume_data.voluem_extent.y;
                    params->volume.extent_z = volume_data.voluem_extent.z;
                    params->scene_accel = {ctx.rg->acceleration_structure(pass_data->scene_accel)};
                    params->sample_randoms = {ctx.rg->buffer(pass_data->sample_randoms)};
                    params->probe_gbuffer_base_color = {ctx.rg->texture(pass_data->probe_gbuffer_base_color)};
                    params->probe_gbuffer_normal_roughness = {ctx.rg->texture(pass_data->probe_gbuffer_normal_roughness)};
                    params->probe_gbuffer_position = {ctx.rg->texture(pass_data->probe_gbuffer_position)};
                    trace_gbuffer_shader_params_[volume_data.index].update_uniform_buffer();
                    ctx.dispatch_rays(
                        camera, trace_gbuffer_shader_, trace_gbuffer_shader_params_[volume_data.index],
                        DdgiContext::num_rays_per_probe, DdgiContext::probes_total_count
                    );
                }
            );
        }

        {
            auto [builder, pass_data] = rg.add_compute_pass<DeferredLightingPassData>(
                fmt::format("DDGI Deferred Lighting #{}", volume_data.index)
            );

            pass_data->probe_gbuffer_base_color = builder.read(trace_gbuffer_base_color);
            pass_data->probe_gbuffer_normal_roughness = builder.read(trace_gbuffer_normal_roughness);
            pass_data->probe_gbuffer_position = builder.read(trace_gbuffer_position);
            pass_data->trace_radiance = builder.write(trace_radiance);
            builder.read(input.shadow_maps.dir_lights_shadow_map);
            builder.read(input.shadow_maps.point_lights_shadow_map);

            builder.set_execution_function<DeferredLightingPassData>(
                [this, &camera, &volume_data](CRef<DeferredLightingPassData> pass_data, gfx::ComputePassContext const& ctx) {
                    auto params = deferred_lighting_shader_params_[volume_data.index].mutable_typed_data<DeferredLightingParams>();
                    params->volume.base_position = volume_data.base_position;
                    params->volume.frame_x = volume_data.frame_x;
                    params->volume.frame_y = volume_data.frame_y;
                    params->volume.frame_z = volume_data.frame_z;
                    params->volume.extent_x = volume_data.voluem_extent.x;
                    params->volume.extent_y = volume_data.voluem_extent.y;
                    params->volume.extent_z = volume_data.voluem_extent.z;
                    params->probe_gbuffer_base_color = {ctx.rg->texture(pass_data->probe_gbuffer_base_color)};
                    params->probe_gbuffer_normal_roughness = {ctx.rg->texture(pass_data->probe_gbuffer_normal_roughness)};
                    params->probe_gbuffer_position = {ctx.rg->texture(pass_data->probe_gbuffer_position)};
                    params->trace_radiance = {ctx.rg->texture(pass_data->trace_radiance)};
                    deferred_lighting_shader_params_[volume_data.index].update_uniform_buffer();
                    ctx.dispatch(
                        camera, deferred_lighting_shader_, deferred_lighting_shader_params_[volume_data.index],
                        ceil_div(DdgiContext::num_rays_per_probe, 32u), ceil_div(DdgiContext::probes_total_count, 8u)
                    );
                }
            );
        }

        auto temp_ddgi_ctx = curr_volume_index == input.ddgi_ctx.num_ddgi_volumes() ? &input.ddgi_ctx : nullptr;

        {
            auto [builder, pass_data] = rg.add_compute_pass<ProbeBlendIrradiancePassData>(
                fmt::format("DDGI Probe Blend Irradiance #{}", volume_data.index)
            );

            pass_data->trace_gbuffer_position = builder.read(trace_gbuffer_position);
            pass_data->trace_radiance = builder.read(trace_radiance);
            pass_data->probe_irradiance = builder.write(irradiance);
            irradiance = pass_data->probe_irradiance;
            if (history_irradiance != gfx::TextureHandle::invalid) {
                pass_data->history_probe_irradiance = builder.read(history_irradiance);
            }

            builder.set_execution_function<ProbeBlendIrradiancePassData>(
                [this, &volume_data, temp_ddgi_ctx](CRef<ProbeBlendIrradiancePassData> pass_data, gfx::ComputePassContext const& ctx) {
                    auto params = probe_blend_irradiance_shader_params_[volume_data.index].mutable_typed_data<ProbeBlendIrradianceParams>();
                    params->volume.base_position = volume_data.base_position;
                    params->volume.frame_x = volume_data.frame_x;
                    params->volume.frame_y = volume_data.frame_y;
                    params->volume.frame_z = volume_data.frame_z;
                    params->volume.extent_x = volume_data.voluem_extent.x;
                    params->volume.extent_y = volume_data.voluem_extent.y;
                    params->volume.extent_z = volume_data.voluem_extent.z;
                    params->trace_gbuffer_position = {ctx.rg->texture(pass_data->trace_gbuffer_position)};
                    params->trace_radiance = {ctx.rg->texture(pass_data->trace_radiance)};
                    params->probe_irradiance = {ctx.rg->texture(pass_data->probe_irradiance), 0, volume_data.index, 1};
                    if (volume_data.prev_index == 0xffffffffu) {
                        params->history_valid = 0;
                        params->history_probe_irradiance = {};
                    } else {
                        params->history_valid = 1;
                        params->history_probe_irradiance = {
                            ctx.rg->texture(pass_data->history_probe_irradiance), 0, 1, volume_data.prev_index, 1,
                        };
                    }
                    probe_blend_irradiance_shader_params_[volume_data.index].update_uniform_buffer();
                    ctx.dispatch(
                        probe_blend_irradiance_shader_, probe_blend_irradiance_shader_params_[volume_data.index],
                        DdgiContext::probes_total_count
                    );

                    if (temp_ddgi_ctx) {
                        temp_ddgi_ctx->set_irradiance_texture(pass_data->probe_irradiance);
                    }
                }
            );
        }

        {
            auto [builder, pass_data] = rg.add_compute_pass<ProbeBlendVisibilityPassData>(
                fmt::format("DDGI Probe Blend Visibility #{}", volume_data.index)
            );

            pass_data->trace_gbuffer_position = builder.read(trace_gbuffer_position);
            pass_data->probe_visibility = builder.write(visibility);
            visibility = pass_data->probe_visibility;
            if (history_visibility != gfx::TextureHandle::invalid) {
                pass_data->history_probe_visibility = builder.read(history_visibility);
            }

            builder.set_execution_function<ProbeBlendVisibilityPassData>(
                [this, &volume_data, temp_ddgi_ctx](CRef<ProbeBlendVisibilityPassData> pass_data, gfx::ComputePassContext const& ctx) {
                    auto params = probe_blend_visibility_shader_params_[volume_data.index].mutable_typed_data<ProbeBlendVisibilityParams>();
                    params->volume.base_position = volume_data.base_position;
                    params->volume.frame_x = volume_data.frame_x;
                    params->volume.frame_y = volume_data.frame_y;
                    params->volume.frame_z = volume_data.frame_z;
                    params->volume.extent_x = volume_data.voluem_extent.x;
                    params->volume.extent_y = volume_data.voluem_extent.y;
                    params->volume.extent_z = volume_data.voluem_extent.z;
                    params->trace_gbuffer_position = {ctx.rg->texture(pass_data->trace_gbuffer_position)};
                    params->probe_visibility = {ctx.rg->texture(pass_data->probe_visibility), 0, volume_data.index, 1};
                    if (volume_data.prev_index == 0xffffffffu) {
                        params->history_valid = 0;
                        params->history_probe_visibility = {};
                    } else {
                        params->history_valid = 1;
                        params->history_probe_visibility = {
                            ctx.rg->texture(pass_data->history_probe_visibility), 0, 1, volume_data.prev_index, 1,
                        };
                    }
                    probe_blend_visibility_shader_params_[volume_data.index].update_uniform_buffer();
                    ctx.dispatch(
                        probe_blend_visibility_shader_, probe_blend_visibility_shader_params_[volume_data.index],
                        DdgiContext::probes_total_count
                    );

                    if (temp_ddgi_ctx) {
                        temp_ddgi_ctx->set_visibility_texture(pass_data->probe_visibility);
                    }
                }
            );
        }
    });

    last_frame_ = this_frame;

    return {
        .irradiance = irradiance,
        .visibility = visibility,
    };
}

}

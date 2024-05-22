#include "ddgi_update.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>
#include <bisemutum/window/window.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(TraceGBufferParams)
    BI_SHADER_PARAMETER(DdgiVolumeShaderData, volume)
    BI_SHADER_PARAMETER(float, ray_length)
    BI_SHADER_PARAMETER_SRV_ACCEL(RaytracingAccelerationStructure, scene_accel)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float2>, sample_randoms)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, probe_gbuffer_base_color, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, probe_gbuffer_normal_roughness, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, probe_gbuffer_position, rhi::ResourceFormat::rgba32_sfloat)
BI_SHADER_PARAMETERS_END()

struct TraceGBufferPassData final {
    gfx::AccelerationStructure scene_accel;
    gfx::TextureHandle probe_gbuffer_base_color;
    gfx::TextureHandle probe_gbuffer_normal_roughness;
    gfx::TextureHandle probe_gbuffer_position;
};

BI_SHADER_PARAMETERS_BEGIN(DeferredLightingParams)
    BI_SHADER_PARAMETER(DdgiVolumeShaderData, volume)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, probe_gbuffer_base_color)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, probe_gbuffer_normal_roughness)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, probe_gbuffer_position)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, trace_radiance, rhi::ResourceFormat::rgba16_sfloat)

    BI_SHADER_PARAMETER_INCLUDE(LightsContextShaderData, lights_ctx)
    BI_SHADER_PARAMETER_INCLUDE(SkyboxContextShaderData, skybox_ctx)

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
    BI_SHADER_PARAMETER(DdgiVolumeShaderData, volume)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, trace_radiance)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, trace_gbuffer_position)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, probe_irradiance, rhi::ResourceFormat::rgba16_sfloat)
BI_SHADER_PARAMETERS_END()

struct ProbeBlendIrradiancePassData final {
    gfx::TextureHandle trace_gbuffer_position;
    gfx::TextureHandle trace_radiance;
    gfx::TextureHandle probe_irradiance;
};

BI_SHADER_PARAMETERS_BEGIN(ProbeBlendVisibilityParams)
    BI_SHADER_PARAMETER(DdgiVolumeShaderData, volume)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, trace_gbuffer_position)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float2>, probe_visibility, rhi::ResourceFormat::rg16_sfloat)
BI_SHADER_PARAMETERS_END()

struct ProbeBlendVisibilityPassData final {
    gfx::TextureHandle trace_gbuffer_position;
    gfx::TextureHandle probe_visibility;
};

} // namespace

DdgiUpdatePass::DdgiUpdatePass() {
    trace_gbuffer_shader_.raygen_source.path = "/bisemutum/shaders/renderer/ddgi/trace_gbufer.hlsl";
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

auto DdgiUpdatePass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> void {
    //
}

}

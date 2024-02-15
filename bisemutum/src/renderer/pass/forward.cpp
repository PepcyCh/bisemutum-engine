#include "forward.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

#include "../context/lights.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(ForwardPassParams)
    BI_SHADER_PARAMETER(uint, num_dir_lights)
    BI_SHADER_PARAMETER(uint, num_point_lights)
    BI_SHADER_PARAMETER(float3, skybox_diffuse_color)
    BI_SHADER_PARAMETER(float3, skybox_specular_color)
    BI_SHADER_PARAMETER(float4x4, skybox_transform)

    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<DirLightData>, dir_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<PointLightData>, point_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float4x4>, dir_lights_shadow_transform)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2DArray, dir_lights_shadow_map)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float4x4>, point_lights_shadow_transform)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2DArray, point_lights_shadow_map)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, shadow_map_sampler)

    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox_diffuse_irradiance)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox_specular_filtered)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, skybox_brdf_lut)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, skybox_sampler)
BI_SHADER_PARAMETERS_END(ForwardPassParams)

struct PassData final {
    gfx::TextureHandle output;
    gfx::TextureHandle velocity;
    gfx::TextureHandle depth;

    gfx::RenderedObjectListHandle list;
};

}

ForwardPass::ForwardPass() {
    fragment_shader_params_.initialize<ForwardPassParams>();

    fragment_shader_.source_path = "/bisemutum/shaders/renderer/forward_pass.hlsl";
    fragment_shader_.source_entry = "forward_pass_fs";
    fragment_shader_.set_shader_params_struct<ForwardPassParams>();
    fragment_shader_.cull_mode = rhi::CullMode::back_face;
}

auto ForwardPass::update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void {
    auto& current_skybox = g_engine->system_manager()->get_system_for_current_scene<SkyboxSystem>()->current_skybox();

    auto params = fragment_shader_params_.mutable_typed_data<ForwardPassParams>();
    params->num_dir_lights = lights_ctx.dir_lights.size();
    params->num_point_lights = lights_ctx.point_lights.size();
    params->skybox_diffuse_color = current_skybox.color * current_skybox.diffuse_strength;
    params->skybox_specular_color = current_skybox.color * current_skybox.specular_strength;
    params->skybox_transform = current_skybox.transform;

    params->dir_lights = {&lights_ctx.dir_lights_buffer, 0};
    params->point_lights = {&lights_ctx.point_lights_buffer, 0};
    params->dir_lights_shadow_transform = {&lights_ctx.dir_lights_shadow_transform_buffer, 0};
    params->dir_lights_shadow_map = {&lights_ctx.dir_lights_shadow_map};
    params->point_lights_shadow_transform = {&lights_ctx.point_lights_shadow_transform_buffer, 0};
    params->point_lights_shadow_map = {&lights_ctx.point_lights_shadow_map};
    params->shadow_map_sampler = {lights_ctx.shadow_map_sampler};

    params->skybox_diffuse_irradiance = {&skybox_ctx.diffuse_irradiance};
    params->skybox_specular_filtered = {&skybox_ctx.specular_filtered};
    params->skybox_brdf_lut = {&skybox_ctx.brdf_lut};
    params->skybox_sampler = {skybox_ctx.skybox_sampler};
}

auto ForwardPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData {
    auto& camera_target = camera.target_texture();

    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("Forward Pass");

    pass_data->output = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::rgba16_sfloat,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->depth = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::d24_unorm_s8_uint,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::depth_stencil_attachment, rhi::TextureUsage::sampled});
    });

    pass_data->output = builder.use_color(
        0,
        gfx::GraphicsPassColorTargetBuilder{pass_data->output}.clear_color()
    );
    pass_data->depth = builder.use_depth_stencil(
        gfx::GraphicsPassDepthStencilTargetBuilder{pass_data->depth}.clear_depth_stencil()
    );

    pass_data->velocity = rg.add_texture([&camera_target](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(
                rhi::ResourceFormat::rg16_sfloat,
                camera_target.desc().extent.width, camera_target.desc().extent.height
            )
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    pass_data->velocity = builder.use_color(
        1,
        gfx::GraphicsPassColorTargetBuilder{pass_data->velocity}.clear_color()
    );

    builder.read(input.shadow_maps.dir_lights_shadow_map);
    builder.read(input.shadow_maps.point_lights_shadow_map);

    builder.read(input.skybox.diffuse_irradiance);
    builder.read(input.skybox.specular_filtered);
    builder.read(input.skybox.brdf_lut);

    pass_data->list = rg.add_rendered_object_list(gfx::RenderedObjectListDesc{
        .camera = camera,
        .fragment_shader = fragment_shader_,
        .type = gfx::RenderedObjectType::all,
        .candidate_drawables = input.drawables,
    });

    fragment_shader_params_.update_uniform_buffer();

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            ctx.render_list(pass_data->list, fragment_shader_params_);
        }
    );

    return OutputData{
        .output = pass_data->output,
        .velocity = pass_data->velocity,
        .depth = pass_data->depth,
    };
}

}

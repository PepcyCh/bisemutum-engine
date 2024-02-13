#include "skybox.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(SkyboxPassParams)
    BI_SHADER_PARAMETER(float3, skybox_color)
    BI_SHADER_PARAMETER(float4x4, skybox_transform)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, skybox_sampler)
BI_SHADER_PARAMETERS_END(SkyboxPassParams)

struct PassData final {
    gfx::TextureHandle output;
    gfx::TextureHandle depth;
};

}

SkyboxPass::SkyboxPass() {
    fragment_shader_params_.initialize<SkyboxPassParams>();

    fragment_shader_.source_path = "/bisemutum/shaders/renderer/skybox_pass.hlsl";
    fragment_shader_.source_entry = "skybox_pass_fs";
    fragment_shader_.set_shader_params_struct<SkyboxPassParams>();
    fragment_shader_.depth_write = false;
    fragment_shader_.depth_compare_op = rhi::CompareOp::less_equal;
}

auto SkyboxPass::update_params(SkyboxContext& skybox_ctx) -> void {
    auto& current_skybox = g_engine->system_manager()->get_system_for_current_scene<SkyboxSystem>()->current_skybox();

    auto params = fragment_shader_params_.mutable_typed_data<SkyboxPassParams>();
    params->skybox_color = current_skybox.color;
    params->skybox_transform = current_skybox.transform;
    params->skybox = {current_skybox.tex};
    params->skybox_sampler = {skybox_ctx.skybox_sampler};
}

auto SkyboxPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData {
    auto [builder, pass_data] = rg.add_graphics_pass<PassData>("Skybox Pass");

    pass_data->output = builder.use_color(0, input.color);
    pass_data->depth = builder.use_depth_stencil(
        gfx::GraphicsPassDepthStencilTargetBuilder{input.depth}//.read_only()
    );

    builder.read(input.skybox.skybox);

    fragment_shader_params_.update_uniform_buffer();

    builder.set_execution_function<PassData>(
        [this, &camera](CRef<PassData> pass_data, gfx::GraphicsPassContext const& ctx) {
            ctx.render_full_screen(camera, fragment_shader_, fragment_shader_params_);
        }
    );

    return {
        .color = pass_data->output,
        .depth = pass_data->depth,
    };
}

}

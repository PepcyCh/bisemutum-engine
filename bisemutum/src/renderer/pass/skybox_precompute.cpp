#include "skybox_precompute.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(SkyboxPrecomputeDiffuseParams)
    BI_SHADER_PARAMETER(uint, size)
    BI_SHADER_PARAMETER(float, inv_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2DArray<float4>, diffuse_irradiance, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, skybox_sampler)
BI_SHADER_PARAMETERS_END(SkyboxPrecomputeDiffuseParams)

struct SkyboxPrecomputePassData final {
    gfx::TextureHandle skybox;
    gfx::TextureHandle diffuse_irradiance;
};

}

SkyboxPrecomputePass::SkyboxPrecomputePass() {
    precompute_diffuse_params.initialize<SkyboxPrecomputeDiffuseParams>();

    precompute_diffuse.source_path = "/bisemutum/shaders/renderer/skybox_precompute_diffuse.hlsl";
    precompute_diffuse.source_entry = "skybox_precompute_diffuse_cs";
    precompute_diffuse.set_shader_params_struct<SkyboxPrecomputeDiffuseParams>();
}

auto SkyboxPrecomputePass::render(gfx::RenderGraph& rg, InputData const& input) -> PrecomputedSkybox {
    PrecomputedSkybox skybox{
        .skybox = rg.import_texture(input.skybox_ctx.skybox_tex.value()),
        .diffuse_irradiance = rg.import_texture(input.skybox_ctx.diffuse_irradiance),
        .specular_filtered = rg.import_texture(input.skybox_ctx.specular_filtered),
        .brdf_lut = rg.import_texture(input.skybox_ctx.brdf_lut),
    };

    if (!last_precomputed_asset || last_precomputed_asset.value() != input.skybox_ctx.skybox_texture_id) {
        auto [builder_diffuse, pd_diffuse] =
            rg.add_compute_pass<SkyboxPrecomputePassData>("Skybox Precompute Diffuse");

        pd_diffuse->skybox = builder_diffuse.read(skybox.skybox);
        pd_diffuse->diffuse_irradiance = builder_diffuse.write(skybox.diffuse_irradiance);

        builder_diffuse.set_execution_function<SkyboxPrecomputePassData>(
            [this, input](CRef<SkyboxPrecomputePassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = precompute_diffuse_params.mutable_typed_data<SkyboxPrecomputeDiffuseParams>();
                params->skybox = {ctx.rg->texture(pass_data->skybox)};
                params->diffuse_irradiance = {ctx.rg->texture(pass_data->diffuse_irradiance)};
                params->skybox_sampler = {input.skybox_ctx.skybox_sampler.value()};
                params->size = input.skybox_ctx.skybox_tex->desc().extent.width;
                params->inv_size = 1.0f / params->size;
                precompute_diffuse_params.update_uniform_buffer();

                auto num_groups = (input.skybox_ctx.diffuse_irradiance.desc().extent.width + 15) / 16;
                ctx.dispatch(precompute_diffuse, precompute_diffuse_params, num_groups, num_groups, 6);
            }
        );

        last_precomputed_asset = input.skybox_ctx.skybox_texture_id;
    }

    return skybox;
}

}

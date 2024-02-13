#include "skybox_precompute.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(SkyboxPrecomputeBrdfParams)
    BI_SHADER_PARAMETER(uint, resolution)
    BI_SHADER_PARAMETER(float, inv_resolution)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float2>, brdf_lut, rhi::ResourceFormat::rg8_unorm)
BI_SHADER_PARAMETERS_END(SkyboxPrecomputeBrdfParams)

struct SkyboxPrecomputeBrdfPassData final {
    gfx::TextureHandle brdf_lut;
};

BI_SHADER_PARAMETERS_BEGIN(SkyboxPrecomputeDiffuseParams)
    BI_SHADER_PARAMETER(uint, tex_size)
    BI_SHADER_PARAMETER(float, inv_tex_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2DArray<float4>, diffuse_irradiance, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, skybox_sampler)
BI_SHADER_PARAMETERS_END(SkyboxPrecomputeDiffuseParams)

struct SkyboxPrecomputeDiffusePassData final {
    gfx::TextureHandle skybox;
    gfx::TextureHandle diffuse_irradiance;
};

BI_SHADER_PARAMETERS_BEGIN(SkyboxPrecomputeSpecularParams)
    BI_SHADER_PARAMETER(uint, tex_size)
    BI_SHADER_PARAMETER(float, inv_tex_size)
    BI_SHADER_PARAMETER(float, roughness)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2DArray<float4>, specular_filtered, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, skybox_sampler)
BI_SHADER_PARAMETERS_END(SkyboxPrecomputeSpecularParams)

struct SkyboxPrecomputeSpecularPassData final {
    gfx::TextureHandle skybox;
    gfx::TextureHandle specular_filtered;
};

}

SkyboxPrecomputePass::SkyboxPrecomputePass() {
    precompute_brdf_lut_params_.initialize<SkyboxPrecomputeBrdfParams>();
    precompute_brdf_lut_.source_path = "/bisemutum/shaders/renderer/ibl_brdf_lut.hlsl";
    precompute_brdf_lut_.source_entry = "ibl_brdf_lut_cs";
    precompute_brdf_lut_.set_shader_params_struct<SkyboxPrecomputeBrdfParams>();

    precompute_diffuse_params_.initialize<SkyboxPrecomputeDiffuseParams>();
    precompute_diffuse_.source_path = "/bisemutum/shaders/renderer/skybox_precompute_diffuse.hlsl";
    precompute_diffuse_.source_entry = "skybox_precompute_diffuse_cs";
    precompute_diffuse_.set_shader_params_struct<SkyboxPrecomputeDiffuseParams>();

    precompute_specular_.source_path = "/bisemutum/shaders/renderer/skybox_precompute_specular.hlsl";
    precompute_specular_.source_entry = "skybox_precompute_specular_cs";
    precompute_specular_.set_shader_params_struct<SkyboxPrecomputeSpecularParams>();
}

auto SkyboxPrecomputePass::render(gfx::RenderGraph& rg, InputData const& input) -> PrecomputedSkybox {
    auto& current_skybox = g_engine->system_manager()->get_system_for_current_scene<SkyboxSystem>()->current_skybox();

    PrecomputedSkybox skybox{
        .skybox = rg.import_texture(current_skybox.tex.value()),
        .diffuse_irradiance = rg.import_texture(input.skybox_ctx.diffuse_irradiance),
        .specular_filtered = rg.import_texture(input.skybox_ctx.specular_filtered),
        .brdf_lut = rg.import_texture(input.skybox_ctx.brdf_lut),
    };

    if (!brdf_lut_valid_) {
        auto [builder, pd] = rg.add_compute_pass<SkyboxPrecomputeBrdfPassData>("IBL BRDF LUT");

        pd->brdf_lut = builder.write(skybox.brdf_lut);

        builder.set_execution_function<SkyboxPrecomputeBrdfPassData>(
            [this, input](CRef<SkyboxPrecomputeBrdfPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = precompute_brdf_lut_params_.mutable_typed_data<SkyboxPrecomputeBrdfParams>();
                params->brdf_lut = {ctx.rg->texture(pass_data->brdf_lut)};
                params->resolution = input.skybox_ctx.brdf_lut.desc().extent.width;
                params->inv_resolution = 1.0f / params->resolution;
                precompute_brdf_lut_params_.update_uniform_buffer();

                auto num_groups = (params->resolution + 15) / 16;
                ctx.dispatch(precompute_brdf_lut_, precompute_brdf_lut_params_, num_groups, num_groups, 1);
            }
        );

        brdf_lut_valid_ = true;
    }

    if (last_precomputed_skybox_ != current_skybox.tex) {
        auto [builder_diffuse, pd_diffuse] =
            rg.add_compute_pass<SkyboxPrecomputeDiffusePassData>("Skybox Precompute Diffuse");

        pd_diffuse->skybox = builder_diffuse.read(skybox.skybox);
        pd_diffuse->diffuse_irradiance = builder_diffuse.write(skybox.diffuse_irradiance);
        skybox.diffuse_irradiance = pd_diffuse->diffuse_irradiance;

        builder_diffuse.set_execution_function<SkyboxPrecomputeDiffusePassData>(
            [this, input](CRef<SkyboxPrecomputeDiffusePassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = precompute_diffuse_params_.mutable_typed_data<SkyboxPrecomputeDiffuseParams>();
                params->skybox = {ctx.rg->texture(pass_data->skybox)};
                params->diffuse_irradiance = {ctx.rg->texture(pass_data->diffuse_irradiance)};
                params->skybox_sampler = {input.skybox_ctx.skybox_sampler.value()};
                params->tex_size = input.skybox_ctx.diffuse_irradiance.desc().extent.width;
                params->inv_tex_size = 1.0f / params->tex_size;
                precompute_diffuse_params_.update_uniform_buffer();

                auto num_groups = (params->tex_size + 15) / 16;
                ctx.dispatch(precompute_diffuse_, precompute_diffuse_params_, num_groups, num_groups, 6);
            }
        );

        auto num_specular_levels = input.skybox_ctx.specular_filtered.desc().levels;
        if (precompute_specular_params_.empty()) {
            precompute_specular_params_.resize(num_specular_levels);
            for (uint32_t i = 0; i < num_specular_levels; i++) {
                precompute_specular_params_[i].initialize<SkyboxPrecomputeSpecularParams>();
            }
        }
        for (uint32_t i = 0; i < num_specular_levels; i++) {
            auto [builder_specular, pd_specular] =
                rg.add_compute_pass<SkyboxPrecomputeSpecularPassData>(fmt::format("Skybox Precompute Specular {}", i));

            pd_specular->skybox = builder_specular.read(skybox.skybox);
            pd_specular->specular_filtered = builder_specular.write(skybox.specular_filtered);
            skybox.specular_filtered = pd_specular->specular_filtered;

            builder_specular.set_execution_function<SkyboxPrecomputeSpecularPassData>(
                [this, i, num_specular_levels, input](
                    CRef<SkyboxPrecomputeSpecularPassData> pass_data, gfx::ComputePassContext const& ctx
                ) {
                    auto params = precompute_specular_params_[i].mutable_typed_data<SkyboxPrecomputeSpecularParams>();
                    params->skybox = {ctx.rg->texture(pass_data->skybox)};
                    params->specular_filtered = {ctx.rg->texture(pass_data->specular_filtered), i};
                    params->skybox_sampler = {input.skybox_ctx.skybox_sampler.value()};
                    params->tex_size = input.skybox_ctx.specular_filtered.desc().extent.width >> i;
                    params->inv_tex_size = 1.0f / params->tex_size;
                    params->roughness = static_cast<float>(i) / (num_specular_levels - 1);
                    precompute_specular_params_[i].update_uniform_buffer();

                    auto num_groups = (params->tex_size + 15) / 16;
                    ctx.dispatch(precompute_specular_, precompute_specular_params_[i], num_groups, num_groups, 6);
                }
            );
        }

        last_precomputed_skybox_ = current_skybox.tex;
    }

    return skybox;
}

}

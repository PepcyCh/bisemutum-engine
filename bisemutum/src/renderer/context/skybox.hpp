#pragma once

#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/sampler.hpp>
#include <bisemutum/graphics/shader_param.hpp>

namespace bi {

BI_SHADER_PARAMETERS_BEGIN(SkyboxContextShaderData)
    BI_SHADER_PARAMETER(float3, skybox_diffuse_color)
    BI_SHADER_PARAMETER(float3, skybox_specular_color)
    BI_SHADER_PARAMETER(float4x4, skybox_transform)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox_diffuse_irradiance)
    BI_SHADER_PARAMETER_SRV_TEXTURE(TextureCube, skybox_specular_filtered)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, skybox_brdf_lut)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, skybox_sampler)
BI_SHADER_PARAMETERS_END()

struct SkyboxContext final {
    SkyboxContext();

    auto update_shader_params() -> void;

    gfx::Texture diffuse_irradiance;
    gfx::Texture specular_filtered;
    gfx::Texture brdf_lut;

    Ptr<gfx::Sampler> skybox_sampler;

    SkyboxContextShaderData shader_data;
};

}

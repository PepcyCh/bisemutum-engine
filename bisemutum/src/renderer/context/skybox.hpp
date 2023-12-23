#pragma once

#include <bisemutum/runtime/asset.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/sampler.hpp>

namespace bi {

struct SkyboxContext final {
    SkyboxContext();

    auto find_skybox() -> void;

    rt::AssetId skybox_texture_id = rt::AssetId::invalid;

    Ptr<gfx::Texture> skybox_tex;
    gfx::Texture diffuse_irradiance;
    gfx::Texture specular_filtered;
    gfx::Texture brdf_lut;

    Ptr<gfx::Sampler> skybox_sampler;
};

}

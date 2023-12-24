#pragma once

#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/sampler.hpp>

namespace bi {

struct SkyboxContext final {
    SkyboxContext();

    gfx::Texture diffuse_irradiance;
    gfx::Texture specular_filtered;
    gfx::Texture brdf_lut;

    Ptr<gfx::Sampler> skybox_sampler;
};

}
